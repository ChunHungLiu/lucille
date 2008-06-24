/*
 *   lucille | Global Illumination renderer
 *
 *             written by Syoyo Fujita.
 *
 */

/*
 * Implements beam tracing algorithm.
 *
 * $Id$
 *
 *  inner node:
 *    - Do packet traversal
 *
 *  leaf node :
 *    - Project triangles in the leaf against domiant axis of the
 *      beam.
 *    
 *    - Clip triangles and beam in 2D domain. 
 *
 *
 *
 * References:
 *
 *   - A Real-time Beam Tracer with Application to Exact Soft Shadows
 *     Ryan Overbeck, Ravi Ramamoorthi and William R. Mark
 *     EuroGraphics Symposium on Rendering, 2007.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "beam.h"
#include "bvh.h"

typedef ri_float_t point2d_t[2];

typedef struct _plane2d_t
{
    point2d_t p;      /* position */
    point2d_t n;      /* normal   */
} plane2d_t;



/* ----------------------------------------------------------------------------
 *
 * Private function definitions
 *
 * ------------------------------------------------------------------------- */

static ri_float_t safeinv( ri_float_t a, ri_float_t eps, ri_float_t val );

/*
 * Clipping routine.
 */

static void intersect(
          point2d_t    i_out,           /* [out]    */
    const point2d_t    s,
    const point2d_t    p,
    const plane2d_t   *b);

static int inside(
          point2d_t    p,
    const plane2d_t   *b);

static void clip(
          point2d_t   *vlist_out,       /* [out]    */
          int         *len_out,         /* [out]    */
    const point2d_t   *vlist_in,
          int          len_in,
    const plane2d_t   *clip_plane);



/* ----------------------------------------------------------------------------
 *
 * Private functions
 *
 * ------------------------------------------------------------------------- */

#define vdot2d( a, b ) ((a)[0] * (b)[0] + (a)[1] * (b)[1])

/*
 *   1.0 / a       if fabs(a) > eps
 *   val           otherwise
 */
ri_float_t 
safeinv( ri_float_t a, ri_float_t eps, ri_float_t val )
{
    if ( fabs(a) > eps ) {
        return 1.0 / a;
    } else {
        return val;
    }
}

void
intersect(
          point2d_t    i_out,       /* [out]    */
    const point2d_t    s,
    const point2d_t    p,
    const plane2d_t   *b)
{
    point2d_t   v;
    ri_float_t  vdotn;
    ri_float_t  sdotn;
    ri_float_t  d;

    ri_float_t  t;


    /*
     * v = direction    
     */
    v[0] = p[0] - s[0];
    v[1] = p[1] - s[1];

    vdotn = vdot2d( v, b->n );
    if (fabs(vdotn) < RI_EPS) {
        printf("Error: vdotn is too small!\n");
        vdotn = 1.0;
    } 

    /*
     * d = plane equation(= -(p . n) )
     */
    d = -( vdot2d( b->p, b->n ) );

    sdotn = vdot2d( s, b->n );
    
    /*
     * t = ray parameter of intersection point.
     */
    t = -(sdotn + d) / vdotn;
    assert(t >= 0.0);

    i_out[0] = s[0] + t * v[0];
    i_out[1] = s[1] + t * v[1];

}

/*
 * Check whether the vertex is inside or not against plane boundary
 * return 1 if the vertex is inside, 0 if not.
 */
int
inside(
          point2d_t  p,
    const plane2d_t *b)
{
    ri_vector_t pb;
    ri_float_t  d;

    vsub( pb, p, b->p );

    // if ((p - b.pos) dot b.normal) >= 0, then p is inside
    d = vdot( pb, b->n );

    if (d >= 0) return 1;
    
    return 0;
}

/*
 * Add the vertex to output.
 */
void
output(
    point2d_t   *list_out,          /* [out]    */
    int         *len_inout,         /* [inout]  */
    point2d_t    p)
{
    int idx;

    idx = (*len_inout);

    vcpy( list_out[idx], p );

    (*len_inout)++;
}


/*
 * Clipper
 */
void
clip(
          point2d_t   *vlist_out,       /* [out]    */
          int         *len_out,         /* [out]    */
    const point2d_t   *vlist_in,
          int          len_in,
    const plane2d_t   *clip_plane)
{

    int                j;
    point2d_t         *s_ptr, *p_ptr;
    point2d_t          newv;

    (*len_out) = 0;

    /* start with last vertex.    */
    s_ptr = (point2d_t *)&vlist_in[len_in - 1];    

    for (j = 0; j < len_in; j++) {

        p_ptr = (point2d_t *)&vlist_in[j];    /* current vertex    */

        if (inside( (*p_ptr), clip_plane )) {

            if (inside( (*s_ptr), clip_plane )) {

                output(vlist_out, len_out, (*p_ptr) );

            } else {
    
                intersect(newv, (*s_ptr), (*p_ptr), clip_plane);
                output(vlist_out, len_out, newv);
                output(vlist_out, len_out, (*p_ptr));
            }

        } else {

            if (inside( (*s_ptr), clip_plane) ) {

                intersect(newv, (*s_ptr), (*p_ptr), clip_plane);
                output(vlist_out, len_out, newv);

            }

        }

        s_ptr = p_ptr;
    }
}

/*
 * Function: ri_bem_set
 *
 *   Setups a beam structure.
 *
 *
 * Parameters:
 *
 *   beam   - Pointer to the beam to be set up. 
 *   org    - Origin of beam.
 *   dir[4] - Corner rays of beam. 
 *
 *
 * Returns:
 *
 *   0 if OK, otherwise if err
 */
int
ri_beam_set(
    ri_beam_t   *beam,      /* [inout]  */
    ri_vector_t  org,
    ri_vector_t  dir[4])
{
    int        i;
    int        mask;
    int        dominant_axis;
    ri_float_t maxval;  

    /*
     * Check if beam's directions lie in same quadrant.
     */
    for (i = 0; i < 3; i++) {
    
        mask  = (dir[0][0] < 0.0) ? 1 : 0;
        mask += (dir[1][0] < 0.0) ? 1 : 0;
        mask += (dir[2][0] < 0.0) ? 1 : 0;
        mask += (dir[3][0] < 0.0) ? 1 : 0;

        if ( (mask != 0) || (mask == 4) ) {

            /* FIXME:
             * split beam so that subdivided beam has same sign.
             */
            fprintf(stderr, "TODO: Beam's dir does not have same sign.\n");
            exit(1);

        }

    }

    vcpy( beam->org,    org    );
    vcpy( beam->dir[0], dir[0] );
    vcpy( beam->dir[1], dir[1] );
    vcpy( beam->dir[2], dir[2] );
    vcpy( beam->dir[3], dir[3] );

    /*
     * Find dominant plane. Use dir[0]
     */
    maxval        = dir[0][0];
    dominant_axis = 0;
    if ( (maxval < dir[0][1]) ) {
        maxval        = dir[0][0];
        dominant_axis = 1;
    }
    if ( (maxval < dir[0][2]) ) {
        maxval        = dir[0][2];
        dominant_axis = 2;
    }
         
    beam->dominant_axis = dominant_axis;

    /*
     * Precompute sign of direction.
     * We know all 4 directions has same sign, thus dir[0] is used to
     * get sign of direction for each axis.
     */
    beam->dirsign[0] = beam->dir[0][0];
    beam->dirsign[1] = beam->dir[0][1];
    beam->dirsign[2] = beam->dir[0][2];

    /*
     * Precompute inverse of direction
     */
    for (i = 0; i < 4; i++) {
        beam->invdir[i][0] = safeinv( beam->dir[i][0], RI_EPS, RI_FLT_MAX );
        beam->invdir[i][1] = safeinv( beam->dir[i][1], RI_EPS, RI_FLT_MAX );
        beam->invdir[i][2] = safeinv( beam->dir[i][2], RI_EPS, RI_FLT_MAX );
    }

    /*
     * Precompute normal plane of beam frustum
     */
    vcross( beam->normal[0], beam->dir[1], beam->dir[0] );
    vcross( beam->normal[1], beam->dir[2], beam->dir[1] );
    vcross( beam->normal[2], beam->dir[3], beam->dir[2] );
    vcross( beam->normal[3], beam->dir[0], beam->dir[1] );

    return 0;   /* OK   */
}



void ri_beam_clip_by_triangle2d(
          ri_beam_t       *outer_out,     /* [out]        */
          ri_beam_t       *inner_out,     /* [out]        */
          int             *nouter_out,    /* [out]        */
          int             *ninner_out,    /* [out]        */
          ri_triangle2d_t *triangle,          /* triangle     */
    const ri_beam_t       *beam)
{

    // plane_t plane;

    point2d_t buf[16];
    int       new_len;
    int       len;


#if 0
    clip

clip(
          ri_vector_t *vlist_out,       /* [out]    */
          int         *len_out,         /* [out]    */
    const ri_vector_t *vlist_in,
          int          len_in,
    const plane_t     *clip_plane)
#endif

}
