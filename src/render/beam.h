/*
 *   lucille | Global Illumination renderer
 *
 *             written by Syoyo Fujita.
 *
 */

/*
 * Beam tracing facility.
 *
 * $Id: beam.h 259 2008-08-01 16:28:06Z syoyo $
 */

#ifndef LUCILLE_BEAM_H
#define LUCILLE_BEAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vector.h"
#include "geom.h"

/*
 * Flags for beam tracing
 */
#define RI_BEAM_MISS_COMPLETELY    0
#define RI_BEAM_HIT_COMPLETELY     1
#define RI_BEAM_HIT_PARTIALLY      2

typedef struct _ri_triangle2d_t {

    ri_float_t  v[3][2];

    ri_geom_t  *geom;
    uint32_t    index;

} ri_triangle2d_t;


/*
 * Beam is defined as a frustum
 * (Beam sharing its origin).
 */
typedef struct _ri_beam_t
{

    ri_vector_t     org;

    /*
     * P[i] - org, where P[i] is lied onto the axis-aligned plane.
     */
    ri_vector_t     dir[4];

    ri_vector_t     length;                     /* Side length(xyz)         */

    ri_float_t      d;                          /* distant to axis-aligned
                                                 * plane                    */

    //i_float_t      t_min;

    /* Farthest t value where intersection was found.  */
    ri_float_t      t_max;

    int             is_tetrahedron;            

    /*
     * Precomputed coefficients
     */
    ri_vector_t     invdir[4];
    int             dominant_axis;
    int             dirsign[3];                 /* xyz                      */
    ri_vector_t     normal[4];

    
    /*
     * Ptr to subdivided beam.
     * Beam is subdivived at the traversal phase.
     */
    struct _ri_beam_t *children;
    int                nchildren;


} ri_beam_t;


extern int  ri_beam_set (       ri_beam_t   *beam,      /* [inout]  */
                                ri_vector_t  org,
                                ri_vector_t  dir[4]);
extern void ri_beam_free(       ri_beam_t   *beam ); 

extern void ri_beam_copy(       ri_beam_t   *dst,
                          const ri_beam_t   *src );

/*
 * Caller should provide enough strage space for outer_out and inner_out.
 */
extern void ri_beam_clip_by_triangle2d(
                                ri_beam_t       *outer_out,     /* [out]    */
                                ri_beam_t       *inner_out,     /* [out]    */
                                int             *nouter_out,    /* [out]    */
                                int             *ninner_out,    /* [out]    */
                                ri_triangle2d_t *triangle2d,
                          const ri_beam_t       *beam);

#ifdef __cplusplus
}
#endif

#endif  /* LUCILLE_BEAM_H */
