/*
 * simple list routine.
 *
 * $Id: list.h,v 1.1.1.1 2004/01/06 13:57:09 syoyo Exp $
 */

#ifndef LIST_H
#define LIST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ri_list_t
{
	void		  *data;
	struct _ri_list_t *next;
	struct _ri_list_t *prev;
} ri_list_t;
	
extern ri_list_t *ri_list_new();
/* add data to last */
extern void	  ri_list_append(ri_list_t *list, void *data);
/* remove data from last */
extern void	  ri_list_remove_last(ri_list_t *list);
extern void	  ri_list_free	(ri_list_t *list);
/* return first element */
extern ri_list_t *ri_list_first (ri_list_t *list);
/* return last element */
extern ri_list_t *ri_list_last  (ri_list_t *list);
/* return next element */
extern ri_list_t *ri_list_next  (ri_list_t *list);
/* return previous element */
extern ri_list_t *ri_list_prev  (ri_list_t *list);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif



