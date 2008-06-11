/*
 *   lucille | Global Illumination renderer
 *
 *             written by Syoyo Fujita.
 *
 */

/*
 * Bounding Volume Hierarchies implementation.
 *
 * References.
 *  
 * - 4-ary BVH by Kimura's thesis.
 *   <http://www.jaist.ac.jp/library/thesis/ks-master-2007/paper/h-kimura/paper.pdf>
 *
 * - Highly Parallel Fast KD-tree Construction for Interactive Ray Tracing
 *   of Dynamic Scenes
 *   Maxim Shevtsov, Alexei Soupikov and Alexander Kapustin.
 *   EUROGRAPHICS 2007
 *   <http://graphics.cs.uni-sb.de/Courses/ss07/sem/index.html>
 *
 * - Ray Tracing Deformable Scenes using Dynamic Bounding Volume Hierarchies
 *   (revised version)
 *   Ingo Wald, Solomon Boulos, and Peter Shirley
 *   Technical Report, SCI Institute, University of Utah, No UUSCI-2006-023
 *   (conditionally accepted at ACM Transactions on Graphics), 2006
 *   <http://www.sci.utah.edu/~wald/Publications/index.html>
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef WITH_SSE
#include <xmmintrin.h>
#include <emmintrin.h>
#endif

#include "bvh.h"
#include "accel.h"
#include "memory.h"
#include "timer.h"
#include "render.h"
#include "log.h"

//#define LOCAL_TEST
#define LOCAL_DEBUG

#define BVH_MAXDEPTH   100
#define BVH_NTRIS_LEAF 4        /* TODO: parameterize.  */


#define BVH_BIN_SIZE  32

/*
 * Buffer for binning to compute approximated SAH 
 */
typedef struct _bvh_bin_buffer_t {

    uint32_t bin[2][3][BVH_BIN_SIZE];  /* (min, max) * xyz * binsize   */

} bvh_bin_buffer_t;

bvh_bin_buffer_t g_binbuf;


typedef struct _triangle4_t {

    ri_vector_t p0x, p0y, p0z;
    ri_vector_t e1x, e1y, e1z;
    ri_vector_t e2x, e2y, e2z;

} triangle4_t;

typedef struct _triangle_t {

    ri_float_t v0x, v0y, v0z;
    ri_float_t v1x, v1y, v1z;
    ri_float_t v2x, v2y, v2z;

    ri_geom_t  *geom;
    uint32_t    index;

} triangle_t;

typedef struct _tri_bbox_t {

    ri_vector_t bmin;
    ri_vector_t bmax;

    uint64_t    index;          // TODO: compaction.

} tri_bbox_t;



typedef struct _interval_t {
    ri_float_t min, max;
} interval_t;

typedef struct _bvh_stack_t {

    ri_qbvh_node_t *node;

} bvh_stack_t;

bvh_stack_t bvh_stack[BVH_MAXDEPTH + 1];
int         bvh_stack_depth;

typedef struct _bvh_stat_t {
    uint64_t ntraversals;
    uint64_t ntested_tris;
    uint64_t nfailed_isects;
} bvh_stat_t;


/*
 * Singleton
 */
bvh_stat_t  g_stat;


/* ----------------------------------------------------------------------------
 *
 * Static functions
 *
 * ------------------------------------------------------------------------- */

static void get_bbox_of_triangle(
    ri_vector_t        bmin_out,            /* [out] */  
    ri_vector_t        bmax_out,            /* [out] */  
    const triangle_t  *triangle);

static void calc_scene_bbox(
    ri_vector_t        bmin_out,            /* [out]    */
    ri_vector_t        bmax_out,            /* [out]    */
    const tri_bbox_t  *tri_bboxes,
    uint64_t           ntriangles);

static void create_triangle_list(
    triangle_t       **triangles_out,       /* [out]    */
    tri_bbox_t       **tri_bboxes_out,      /* [out]    */
    uint64_t          *ntriangles,          /* [out]    */
    const ri_list_t   *geom_list);

static int bin_triangle_edge(
    bvh_bin_buffer_t  *binbuf,              /* [inout]  */
    const ri_vector_t  scene_bmin,
    const ri_vector_t  scene_bmax,
    const tri_bbox_t  *tri_bboxes,
    uint64_t           ntriangles);

static void calc_bbox_of_triangles(
    ri_vector_t        bmin_out,            /* [out]    */
    ri_vector_t        bmax_out,            /* [out]    */
    const tri_bbox_t  *tri_bboxes,
    uint64_t           ntriangles);

static int bvh_construct(
    ri_qbvh_node_t    *root,
    ri_vector_t        bmin,
    ri_vector_t        bmax,
    triangle_t        *triangles,
    triangle_t        *triangles_buf,
    tri_bbox_t        *tri_bboxes,
    tri_bbox_t        *tri_bboxes_buf,
    uint64_t           index_left,          /* [index_left, index_right)    */
    uint64_t           index_right);

ri_qbvh_node_t *
ri_qbvh_node_new()
{
    return (ri_qbvh_node_t *)ri_mem_alloc(sizeof(ri_qbvh_node_t));
}

/* ----------------------------------------------------------------------------
 *
 * Public functions
 *
 * ------------------------------------------------------------------------- */

/*
 * Function: ri_bvh_build
 *
 *     Builds Bounded Volume Hierarchy data structure for accelerated ray tracing.
 *
 * Parameters:
 *
 *     scenegeoms - geometry in the scene.
 *     method     - bvh construction method. default is BVH_MEDIAN
 *
 * Returns:
 *
 *     Built BVH data strucure.
 */
void *
ri_bvh_build(
    const void *data)
{
    ri_bvh_t           *bvh;
    ri_qbvh_node_t     *root;
    ri_timer_t         *tm;
    ri_vector_t         bmin, bmax;
    ri_float_t          eps = 0.00001;

    ri_scene_t         *scene = (ri_scene_t *)data;
    
    triangle_t         *triangles;
    triangle_t         *triangles_buf;          /* temporal buffer  */
    tri_bbox_t         *tri_bboxes;
    tri_bbox_t         *tri_bboxes_buf;         /* temporal buffer  */
    uint64_t            ntriangles;

    tm = ri_render_get()->context->timer;

    ri_log( LOG_INFO, "Building BVH ... " );
    ri_timer_start( tm, "BVH Construction" );

    bvh = ( ri_bvh_t * )ri_mem_alloc( sizeof( ri_bvh_t ) );


    /*
     * 1. Create 1D array of triangle and its bbox.
     */
    {
        create_triangle_list(&triangles,
                             &tri_bboxes,
                             &ntriangles,
                              scene->geom_list);

        tri_bboxes_buf = ri_mem_alloc(sizeof(tri_bbox_t) * ntriangles);
        ri_mem_copy(tri_bboxes_buf, tri_bboxes, sizeof(tri_bbox_t)*ntriangles);

        triangles_buf = ri_mem_alloc(sizeof(triangle_t) * ntriangles);
        ri_mem_copy(triangles_buf, triangles, sizeof(triangle_t)*ntriangles);
    }

    /*
     * 2. Calculate bounding box of the scene.
     */
    {
        calc_scene_bbox( bmin, bmax, tri_bboxes, ntriangles );

        bvh->bmin[0] = bmin[0] * (1.0 - eps);
        bvh->bmin[1] = bmin[1] * (1.0 - eps);
        bvh->bmin[2] = bmin[2] * (1.0 - eps);

        bvh->bmax[0] = bmax[0] * (1.0 + eps);
        bvh->bmax[1] = bmax[1] * (1.0 + eps);
        bvh->bmax[2] = bmax[2] * (1.0 + eps);

        ri_log(LOG_INFO, "  bmin (%f, %f, %f)",
            bvh->bmin[0], bvh->bmin[1], bvh->bmin[2]);
        ri_log(LOG_INFO, "  bmax (%f, %f, %f)",
            bvh->bmax[0], bvh->bmax[1], bvh->bmax[2]);
    }
    
    ri_log(LOG_INFO, "  # of tris = %d", ntriangles);


    /*
     * 3. Construct BVH.
     */
    root      = ri_qbvh_node_new();
    bvh->root = root;
    
    bvh_construct(
        bvh->root,
        bvh->bmin,
        bvh->bmax,
        triangles,
        triangles_buf,
        tri_bboxes,
        tri_bboxes_buf,
        0,
        ntriangles);


    ri_mem_free( triangles_buf );
    ri_mem_free( tri_bboxes );
    ri_mem_free( tri_bboxes_buf );

    ri_timer_end( tm, "BVH Construction" );

    ri_log( LOG_INFO, "BVH Construction time: %f sec",
           ri_timer_elapsed( tm, "BVH Construction" ) );

    ri_log( LOG_INFO, "Built BVH." );

    return (void *)bvh;
}

void
ri_bvh_free( void *accel )
{
    ri_bvh_t *bvh = (ri_bvh_t *)accel;

    ri_mem_free(bvh);
}

int
ri_bvh_intersect(
    void                    *accel,
    ri_ray_t                *ray,
    ri_intersection_state_t *state)
{
    ri_bvh_t *bvh = (ri_bvh_t *)accel;

    (void)bvh;
    (void)ray;
    (void)state;

    return 0;
}



/* ---------------------------------------------------------------------------
 *
 * Private functions
 *
 * ------------------------------------------------------------------------ */

static inline ri_float_t
calc_surface_area(
    ri_vector_t bmin,
    ri_vector_t bmax)
{
    ri_float_t sa;

    assert( bmax[0] >= bmin[0] );
    assert( bmax[1] >= bmin[1] );
    assert( bmax[2] >= bmin[2] );

    sa = (bmax[0] - bmin[0]) * (bmax[1] - bmin[1]) +
         (bmax[1] - bmin[1]) * (bmax[2] - bmin[2]) +
         (bmax[2] - bmin[2]) * (bmax[0] - bmin[0]);

    sa *= 2.0;

    return sa;
}

static inline ri_float_t
SAH(
    int        ns1,
    ri_float_t left_area,
    int        ns2,
    ri_float_t right_area,
    ri_float_t s)
{
    // param
    const float Taabb = 0.2f;
    const float Ttri  = 0.8f;

    float T;

    T = 2.0f * Taabb
      + (left_area / s) * (ri_float_t)ns1 * Ttri
      + (right_area / s) * (ri_float_t)ns2 * Ttri; 

    return T;
}

int 
find_cut_from_bin(
    ri_float_t             *cut_pos_out,        /* [out]    */
    int                    *cut_axis_out,       /* [out]    */ 
    const bvh_bin_buffer_t *binbuf,
    ri_vector_t             bmin,
    ri_vector_t             bmax,
    uint64_t                ntriangles)
{
    int         i, j;

    ri_float_t  cost;
    int         min_cost_axis = 0;
    uint32_t    min_cost_idx  = 0;
    ri_float_t  min_cost_pos  = 0.0;
    ri_float_t  min_cost      = RI_INFINITY;
        
    uint64_t    tris_left; 
    uint64_t    tris_right;

    ri_vector_t bsize;
    ri_vector_t bstep;

    ri_vector_t bmin_left, bmax_left;
    ri_vector_t bmin_right, bmax_right;
    ri_float_t  sa_left, sa_right, sa_total;

    ri_float_t  pos;

    bsize[0]    = bmax[0] - bmin[0];
    bsize[1]    = bmax[1] - bmin[1];
    bsize[2]    = bmax[2] - bmin[2];

    bstep[0] = bsize[0] / (ri_float_t)BVH_BIN_SIZE;
    bstep[1] = bsize[1] / (ri_float_t)BVH_BIN_SIZE;
    bstep[2] = bsize[2] / (ri_float_t)BVH_BIN_SIZE;

    sa_total    = calc_surface_area( bmin, bmax );

    for (j = 0; j < 3; j++) {   // axis

        /*
         *  Compute SAH cost for right side of bbox cell.
         *  Exclude both extreme side of bbox.
         *     
         *  i:      0    1    2    3    
         *     +----+----+----+----+----+
         *     |    |    |    |    |    |
         *     +----+----+----+----+----+
         *
         */

        tris_left  = 0; 
        tris_right = ntriangles;

        vcpy( bmin_left, bmin ); vcpy( bmin_right, bmin );
        vcpy( bmax_left, bmax ); vcpy( bmax_right, bmax );

        for (i = 0; i < BVH_BIN_SIZE - 1; i++) {

            tris_left  += binbuf->bin[0][j][i];
            tris_right -= binbuf->bin[1][j][i];

            // printf("[%d], left = %lld, right = %lld\n", i, tris_left, tris_right);

            //assert(tris_left  <= tris_right);
            assert(tris_left  <=  ntriangles);
            assert(tris_right <=  ntriangles);

            /*
             * split pos = bmin + (i + 1) * (bsize / BIN_SIZE)
             * (i + 1) because we want right side of the cell.
             */
            

            pos = bmin[j] + (i + 1) * bstep[j];

            printf("pos = %f\n", pos);
            
            bmax_left[j]  = pos;
            bmin_right[j] = pos;

            sa_left  = calc_surface_area( bmin_left, bmax_left );
            sa_right = calc_surface_area( bmin_right, bmax_right );

            cost = SAH( tris_left, sa_left, tris_right, sa_right, sa_total );
            printf("cost [%d][%d] = %f\n", j, i, cost);
            
            if (cost < min_cost) {
                min_cost      = cost;
                min_cost_axis = j;
                min_cost_idx  = i + 1;  // TODO: remove
                min_cost_pos  = pos; 
            }
        }

    }

    (*cut_axis_out) = min_cost_axis;
    (*cut_pos_out)  = min_cost_pos;

    printf("min_cost_axis = %d\n", min_cost_axis);
    printf("min_cost_pos  = %f\n", min_cost_pos);

    return 0;   /* OK */
   
}

int
bvh_construct(
    ri_qbvh_node_t *root,
    ri_vector_t     bmin,
    ri_vector_t     bmax,
    triangle_t     *triangles,
    triangle_t     *triangles_buf,
    tri_bbox_t     *tri_bboxes,
    tri_bbox_t     *tri_bboxes_buf,
    uint64_t        index_left,             /* [index_left, index_right)    */
    uint64_t        index_right)
{

    uint64_t n;

    n = index_right - index_left;

    ri_float_t cut_pos;
    int        cut_axis;

    /*
     * 1. If # of triangles are less than threshold, make a leaf node.
     */
    if (n < BVH_NTRIS_LEAF) {

        root->child[0] = NULL;      // TODO: ptr to triangle list.
        root->child[1] = NULL;      // TODO: # of triangles.

        (void)triangles;

        root->is_leaf = 1;

        return 0;
    }


    /*
     * 2. Bin edges of triangles.
     */
    {
        bin_triangle_edge(
            &g_binbuf,
            bmin,
            bmax,
            tri_bboxes + index_left, 
            n);
             

        find_cut_from_bin(
            &cut_pos,
            &cut_axis,
            &g_binbuf,
             bmin,
             bmax,
             n);
    }

    uint64_t i;
    uint64_t ntris_left  = 0;
    uint64_t ntris_right = n - 1;

    /*
     * 3. Partition triangles into left and right.
     * 
     * bbox data is read from tri_bboxes_buf, then left-right separated
     * bbox data is wrote to tri_bboxes.
     */
    {
        memcpy(tri_bboxes_buf, tri_bboxes + index_left, sizeof(tri_bbox_t) * n);

        for (i = 0; i < n; i++) {

            if (tri_bboxes_buf[i].bmax[cut_axis] < cut_pos) {

                /* left   */

                assert(ntris_left < n);

                memcpy(tri_bboxes + index_left + ntris_left,
                       tri_bboxes_buf + i,
                       sizeof(tri_bbox_t));

                ntris_left++;
        
            } else {

                /* right    */

                assert(ntris_right >= 0);

                memcpy(tri_bboxes + index_left + ntris_right,
                       tri_bboxes_buf + i,
                       sizeof(tri_bbox_t));

                ntris_right--;

            }

        }

        printf("nleft = %d, nright = %d\n", ntris_left, ntris_right);

        if( ntris_left == 0 || ntris_left == n ) {

            /* Couldn't partition bboxes into left and right.
             * Force split at object median.
             */
            ntris_left = n / 2;
            
        }

    }


    /*
     * 4. Subdivide.
     */
    {
        ri_vector_t bmin_left,  bmax_left;
        ri_vector_t bmin_right, bmax_right;
        ri_qbvh_node_t *node_left, *node_right;


        node_left      = ri_qbvh_node_new();
        root->child[0] = node_left;

        node_right     = ri_qbvh_node_new();
        root->child[1] = node_right;

        /*
         * left
         */
        calc_bbox_of_triangles(
            bmin_left,
            bmax_left, 
            tri_bboxes + index_left,
            ntris_left);

        root->bbox[0] = bmin_left[0];
        root->bbox[1] = bmin_left[1];
        root->bbox[2] = bmin_left[2];
        root->bbox[3] = bmax_left[0];
        root->bbox[4] = bmax_left[1];
        root->bbox[5] = bmax_left[2];

        printf("left: (%f, %f, %f)-(%f, %f, %f)\n",
                bmin_left[0], bmin_left[1], bmin_left[2],
                bmax_left[0], bmax_left[1], bmax_left[2]);

        bvh_construct( 
            node_left,
            bmin_left,
            bmax_left,
            triangles,
            triangles_buf,
            tri_bboxes,
            tri_bboxes_buf,
            index_left,
            index_left + ntris_left);


        /*
         * right
         */
        calc_bbox_of_triangles(
            bmin_right,
            bmax_right, 
            tri_bboxes + index_left + ntris_left,
            n - ntris_left);

        root->bbox[6+0] = bmin_right[0];
        root->bbox[6+1] = bmin_right[1];
        root->bbox[6+2] = bmin_right[2];
        root->bbox[6+3] = bmax_right[0];
        root->bbox[6+4] = bmax_right[1];
        root->bbox[6+5] = bmax_right[2];

        bvh_construct( 
            node_right,
            bmin_right,
            bmax_right,
            triangles,
            triangles_buf,
            tri_bboxes,
            tri_bboxes_buf,
            index_left + ntris_left,
            index_right);

    }

    return 0;   /* OK */
}



/*
 * Record the edge of triangle into the bin buffer.
 */
int 
bin_triangle_edge(
    bvh_bin_buffer_t  *binbuf,          /* [inout] */
    const ri_vector_t  scene_bmin,
    const ri_vector_t  scene_bmax,
    const tri_bbox_t  *tri_bboxes,
    uint64_t           ntriangles)
{
    uint64_t i;

    ri_vector_t bmin;
    ri_vector_t bmax;

    ri_vector_t quantized_bmin;
    ri_vector_t quantized_bmax;

    ri_float_t binsize = (ri_float_t)BVH_BIN_SIZE;

    /*
     * calculate extent
     */
    ri_vector_t scene_size;
    ri_vector_t scene_invsize;

    {

        scene_size[0] = scene_bmax[0] - scene_bmin[0];
        scene_size[1] = scene_bmax[1] - scene_bmin[1];
        scene_size[2] = scene_bmax[2] - scene_bmin[2];


        // [0, BIN_SIZE)
        assert(scene_size[0] > 0.0);
        assert(scene_size[1] > 0.0);
        assert(scene_size[2] > 0.0);
        scene_invsize[0] = binsize / scene_size[0];
        scene_invsize[1] = binsize / scene_size[1];
        scene_invsize[2] = binsize / scene_size[2];
    }

    // clear bin data
    memset(binbuf, 0, sizeof(bvh_bin_buffer_t));


    uint32_t idx_bmin[3];
    uint32_t idx_bmax[3];

    for (i = 0; i < ntriangles; i++) {

        /*
         * Quantize the position into [0, BIN_SIZE)
         *
         *  q[i] = (int)(p[i] - scene_bmin) / scene_size
         */

        vcpy(bmin, tri_bboxes[i].bmin);
        vcpy(bmax, tri_bboxes[i].bmax);

        printf("bmin = %f, %f, %f\n", bmin[0], bmin[1], bmin[2]);
        printf("bmax = %f, %f, %f\n", bmax[0], bmax[1], bmax[2]);

        quantized_bmin[0] = (bmin[0] - scene_bmin[0]) * scene_invsize[0];
        quantized_bmin[1] = (bmin[1] - scene_bmin[1]) * scene_invsize[1];
        quantized_bmin[2] = (bmin[2] - scene_bmin[2]) * scene_invsize[2];

        quantized_bmax[0] = (bmax[0] - scene_bmin[0]) * scene_invsize[0];
        quantized_bmax[1] = (bmax[1] - scene_bmin[1]) * scene_invsize[1];
        quantized_bmax[2] = (bmax[2] - scene_bmin[2]) * scene_invsize[2];

        /* idx is now in [0, BIN_SIZE) */
        idx_bmin[0] = (uint32_t)(quantized_bmin[0]);
        idx_bmin[1] = (uint32_t)(quantized_bmin[1]);
        idx_bmin[2] = (uint32_t)(quantized_bmin[2]);

        idx_bmax[0] = (uint32_t)(quantized_bmax[0]);
        idx_bmax[1] = (uint32_t)(quantized_bmax[1]);
        idx_bmax[2] = (uint32_t)(quantized_bmax[2]);

        // printf("min = %d, %d, %d, max = %d, %d, %d\n",
        //     idx_bmin[0], idx_bmin[1], idx_bmin[2],
        //     idx_bmax[0], idx_bmax[1], idx_bmax[2]);

        if (idx_bmin[0] >= BVH_BIN_SIZE) idx_bmin[0] = BVH_BIN_SIZE - 1;
        if (idx_bmin[1] >= BVH_BIN_SIZE) idx_bmin[1] = BVH_BIN_SIZE - 1;
        if (idx_bmin[2] >= BVH_BIN_SIZE) idx_bmin[2] = BVH_BIN_SIZE - 1;
        if (idx_bmax[0] >= BVH_BIN_SIZE) idx_bmax[0] = BVH_BIN_SIZE - 1;
        if (idx_bmax[1] >= BVH_BIN_SIZE) idx_bmax[1] = BVH_BIN_SIZE - 1;
        if (idx_bmax[2] >= BVH_BIN_SIZE) idx_bmax[2] = BVH_BIN_SIZE - 1;
            
        assert(idx_bmin[0] < BVH_BIN_SIZE);
        assert(idx_bmin[1] < BVH_BIN_SIZE);
        assert(idx_bmin[2] < BVH_BIN_SIZE);

        assert(idx_bmax[0] < BVH_BIN_SIZE);
        assert(idx_bmax[1] < BVH_BIN_SIZE);
        assert(idx_bmax[2] < BVH_BIN_SIZE);

        /* Increment bin counter */
        binbuf->bin[0][0][idx_bmin[0]]++;
        binbuf->bin[0][1][idx_bmin[1]]++;
        binbuf->bin[0][2][idx_bmin[2]]++;

        binbuf->bin[1][0][idx_bmax[0]]++;
        binbuf->bin[1][1][idx_bmax[1]]++;
        binbuf->bin[1][2][idx_bmax[2]]++;

    }

    return 0;   // OK

}


/*
 * Create an array of triangles and its bbox from the list of geometory.
 */
void
create_triangle_list(
    triangle_t       **triangles_out,       /* [out] */
    tri_bbox_t       **tri_bboxes_out,      /* [out] */
    uint64_t          *ntriangles,          /* [out] */
    const ri_list_t   *geom_list)
{
    uint32_t     i;
    ri_vector_t  v[3];
    ri_list_t   *itr;
    ri_geom_t   *geom;

    uint64_t     n = 0;
    uint64_t     idx;

    assert(geom_list != NULL);

    /*
     * find # of triangles in the geometry list.
     */

    for (itr  = ri_list_first( (ri_list_t *)geom_list );
         itr != NULL;
         itr  = ri_list_next( itr ) ) {

        geom = ( ri_geom_t * )itr->data;

        assert(geom != NULL);

        n += geom->nindices / 3;

    }

    (*triangles_out)  = ri_mem_alloc(sizeof(triangle_t) * n);
    (*tri_bboxes_out) = ri_mem_alloc(sizeof(tri_bbox_t) * n);
    (*ntriangles)     = n;


    /*
     * Construct array of triangles and array of bbox of triangles.
     */

    ri_vector_t bmin, bmax;

    idx = 0;

    for (itr  = ri_list_first( (ri_list_t *)geom_list );
         itr != NULL;
         itr  = ri_list_next( itr ) ) {

        geom = ( ri_geom_t * )itr->data;

        for (i = 0; i < geom->nindices / 3; i++) {

            vcpy(v[0], geom->positions[geom->indices[3 * i + 0]]);    
            vcpy(v[1], geom->positions[geom->indices[3 * i + 1]]);    
            vcpy(v[2], geom->positions[geom->indices[3 * i + 2]]);    

            (*triangles_out)[idx].v0x = v[0][0];
            (*triangles_out)[idx].v0y = v[0][1];
            (*triangles_out)[idx].v0z = v[0][2];

            (*triangles_out)[idx].v1x = v[1][0];
            (*triangles_out)[idx].v1y = v[1][1];
            (*triangles_out)[idx].v1z = v[1][2];

            (*triangles_out)[idx].v2x = v[2][0];
            (*triangles_out)[idx].v2y = v[2][1];
            (*triangles_out)[idx].v2z = v[2][2];

            (*triangles_out)[idx].geom  = geom;
            (*triangles_out)[idx].index = 3 * i;


            get_bbox_of_triangle( bmin, bmax, &((*triangles_out)[idx]) );

            vcpy( (*tri_bboxes_out)[idx].bmin, bmin );
            vcpy( (*tri_bboxes_out)[idx].bmax, bmax );
            (*tri_bboxes_out)[idx].index = idx;

            idx++;

        }

    }
}


static void
calc_scene_bbox(
    ri_vector_t        bmin_out,        /* [out] */
    ri_vector_t        bmax_out,        /* [out] */
    const tri_bbox_t  *tri_bboxes,
    uint64_t           ntriangles)
{
    uint64_t i;

    assert(tri_bboxes != NULL);

    vcpy( bmin_out, tri_bboxes[0].bmin );
    vcpy( bmax_out, tri_bboxes[0].bmax );

    for (i = 1; i < ntriangles; i++) {

        vmin( bmin_out, bmin_out, tri_bboxes[i].bmin );
        vmax( bmax_out, bmax_out, tri_bboxes[i].bmax );

    }
}


static void
get_bbox_of_triangle(
    ri_vector_t        bmin_out,         /* [out] */  
    ri_vector_t        bmax_out,         /* [out] */  
    const triangle_t  *triangle)
{
    bmin_out[0] = triangle->v0x; 
    bmin_out[1] = triangle->v0y; 
    bmin_out[2] = triangle->v0z; 
    bmax_out[0] = triangle->v0x; 
    bmax_out[1] = triangle->v0y; 
    bmax_out[2] = triangle->v0z; 

    bmin_out[0] = (bmin_out[0] < triangle->v1x) ? bmin_out[0] : triangle->v1x;
    bmin_out[1] = (bmin_out[1] < triangle->v1y) ? bmin_out[1] : triangle->v1y;
    bmin_out[2] = (bmin_out[2] < triangle->v1z) ? bmin_out[2] : triangle->v1z;

    bmin_out[0] = (bmin_out[0] < triangle->v2x) ? bmin_out[0] : triangle->v2x;
    bmin_out[1] = (bmin_out[1] < triangle->v2y) ? bmin_out[1] : triangle->v2y;
    bmin_out[2] = (bmin_out[2] < triangle->v2z) ? bmin_out[2] : triangle->v2z;

    bmax_out[0] = (bmax_out[0] > triangle->v1x) ? bmax_out[0] : triangle->v1x;
    bmax_out[1] = (bmax_out[1] > triangle->v1y) ? bmax_out[1] : triangle->v1y;
    bmax_out[2] = (bmax_out[2] > triangle->v1z) ? bmax_out[2] : triangle->v1z;

    bmax_out[0] = (bmax_out[0] > triangle->v2x) ? bmax_out[0] : triangle->v2x;
    bmax_out[1] = (bmax_out[1] > triangle->v2y) ? bmax_out[1] : triangle->v2y;
    bmax_out[2] = (bmax_out[2] > triangle->v2z) ? bmax_out[2] : triangle->v2z;
}

static void
calc_bbox_of_triangles(
    ri_vector_t        bmin_out,         /* [out] */  
    ri_vector_t        bmax_out,         /* [out] */  
    const tri_bbox_t  *tri_bboxes,
    uint64_t           ntriangles)
{
    uint64_t i;

    ri_vector_t bmin;
    ri_vector_t bmax;

    vcpy( bmin, tri_bboxes[0].bmin );
    vcpy( bmax, tri_bboxes[0].bmax );

    for (i = 1; i < ntriangles; i++) {

        vmin( bmin, bmin, tri_bboxes[i].bmin );
        vmax( bmax, bmax, tri_bboxes[i].bmax );

    }

    vcpy( bmin_out, bmin );
    vcpy( bmax_out, bmax );

}

int
gather_triangles(
    triangle_t       *triangles_to_out,    /* [out]    */
    const triangle_t *triangles_from,
    const tri_bbox_t *tri_bboxes,
    uint64_t          ntriangles)
{
    uint64_t i;
    uint64_t j;

    for (i = 0; i < ntriangles; i++) {

        j = tri_bboxes[i].index;

        memcpy( triangles_to_out + i, triangles_from + j, sizeof(triangle_t));
        

    }

    return 0;   /* OK */
}

#if 0
static inline void
get_bbox_of_triangle4(
    ri_vector_t        bminx_out,        /* [out] */  
    ri_vector_t        bminy_out,        /* [out] */  
    ri_vector_t        bminz_out,        /* [out] */  
    ri_vector_t        bmaxx_out,        /* [out] */  
    ri_vector_t        bmaxy_out,        /* [out] */  
    ri_vector_t        bmaxz_out,        /* [out] */  
    const triangle4_t *triangles)
{
    vec bminx, bminy, bminz;
    vec bmaxx, bmaxy, bmaxz;

    vec v0x; vcpy(v0x, triangles->v0x);
    vec v1x; vcpy(v1x, triangles->v1x);
    vec v2x; vcpy(v2x, triangles->v2x);

    vec v0y; vcpy(v0y, triangles->v0y);
    vec v1y; vcpy(v1y, triangles->v1y);
    vec v2y; vcpy(v2y, triangles->v2y);

    vec v0z; vcpy(v0z, triangles->v0z);
    vec v1z; vcpy(v1z, triangles->v1z);
    vec v2z; vcpy(v2z, triangles->v2z);

    vcpy(bminx, v0x);
    vcpy(bminy, v0y);
    vcpy(bminz, v0z);

    vcpy(bmaxx, v0x);
    vcpy(bmaxy, v0y);
    vcpy(bmaxz, v0z);

    vmin(bminx, bminx, v1x);
    vmin(bminy, bminy, v1y);
    vmin(bminz, bminz, v1z);

    vmin(bminx, bminx, v2x);
    vmin(bminy, bminy, v2y);
    vmin(bminz, bminz, v2z);

    vmax(bmaxx, bmaxx, v1x);
    vmax(bmaxy, bmaxy, v1y);
    vmax(bmaxz, bmaxz, v1z);

    vmax(bmaxx, bmaxx, v2x);
    vmax(bmaxy, bmaxy, v2y);
    vmax(bmaxz, bmaxz, v2z);

    vcpy(bminx_out, bminx);
    vcpy(bminy_out, bminy);
    vcpy(bminz_out, bminz);

    vcpy(bmaxx_out, bmaxx);
    vcpy(bmaxy_out, bmaxy);
    vcpy(bmaxz_out, bmaxz);

}
#endif
