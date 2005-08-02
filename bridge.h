/* $Id$ */

/** @file bridge.h Header file for bridges */

#ifndef BRIDGE_H
#define BRIDGE_H

/** Struct containing information about a single bridge type
 */
typedef struct Bridge {
	byte avail_year;     ///< the year in which the bridge becomes available
	byte min_length;     ///< the minimum length of the bridge (not counting start and end tile)
	byte max_length;     ///< the maximum length of the bridge (not counting start and end tile)
	uint16 price;        ///< the relative price of the bridge
	uint16 speed;        ///< maximum travel speed
	PalSpriteID sprite;  ///< the sprite which is used in the GUI (possibly with a recolor sprite)
	StringID material;   ///< the string that contains the bridge description
} Bridge;

extern const Bridge _bridge[MAX_BRIDGES];

#endif /* BRIDGE_H */
