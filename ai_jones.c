#include <arpa/inet.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Data.h>

#include "tpe.h"
#include "ai_util.h"
#include "tpe_event.h"
#include "tpe_msg.h"
#include "tpe_orders.h"
#include "tpe_obj.h"
#include "tpe_ship.h"
#include "tpe_util.h"


/** General AI struture for Jones
 */
struct ai {
	/** Pointer back to TPE structure */
	struct tpe *tpe;
};

static int jones_order_planet(void *data, int type, void *event);

TPE_AI("jones", "Jones - Aggresive Minisec AI", ai_jones_init)

struct ai *
ai_jones_init(struct tpe *tpe){
	struct ai *ai;
	
	ai = calloc(1,sizeof(struct ai));
	ai->tpe = tpe;

	tpe_event_handler_add(tpe->event, "PlanetNoOrders",
			jones_order_planet, ai);

	return ai;
}

/**
 * Jones should really only need one planet
 */
static int
jones_order_planet(void *aiv, int type, void *planetv){
	struct ai *jones = aiv;
	struct object *planet;
	int buildid;

	planet = planetv;

	buildid = tpe_order_get_type_by_name(jones->tpe, "BuildFleet");
	if (buildid == -1) return 1;
	

	return 1;
}