/* safe_strncpy

	Copies at most   max_length   characters, stopping at the first   \0
   character in   source.   The algorithm correctly handles overlapping
   buffer areas. */

#include "std2/includes.h"
#include "std2/safe_strncpy.h"

char *safe_strncpy (char * dest,const char * source,int  max_length)
{

/* There are several conditions to be considered in determining buffer
   area overlap:

   Buffer Overlap?		Picture		Direction In Which To Copy
---------------------------------------------------------------------------
1. dest == source	   | dest/src |		  no copy necessary
			   ============

2. tail of dest against	  |   dest | | src   |	  left to right
   head of source	  ---------===--------

3. head of dest against	  |   src | | dest   |	  right to left
   tail of source	  --------===---------

4. no overlap	|src| |dest|   or   |dest| |src|  either direction
		----- ------	    ------ -----
*/

    char *ret_val = dest;
    int real_length;

    if (source == NULL || dest == NULL)
	return NULL;

/* Compute the actual length of the text to be copied */

    for (real_length = 0; real_length < max_length && source[real_length];
	    real_length++);

/* Account for condition 3,  dest head v. source tail */

    if (source + real_length >= dest && source < dest)
	for (; real_length >= 0; real_length--)
	    dest[real_length] = source[real_length];

/* Account for conditions 2 and 4,  dest tail v. source head  or no overlap */

    else if (source != dest)
	for (; real_length >= 0; real_length--)
	    *dest++ = *source++;

/* Implicitly handle condition 1, by not performing the copy */

    return ret_val;
} /* safe_strncpy */

