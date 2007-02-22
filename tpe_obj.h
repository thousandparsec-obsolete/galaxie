struct tpe;
struct tpe_obj;
struct tpe_gui_obj;
struct object;

enum objtype {
	OBJTYPE_UNIVERSE,
	OBJTYPE_GALAXY,
	OBJTYPE_SYSTEM, /* Star System */
	OBJTYPE_PLANET, /* Planet */
	OBJTYPE_FLEET, 
};

struct vector {
	uint64_t x,y,z;
};

struct planet_resource {
	uint32_t rid;
	uint32_t surface;
	uint32_t minable;
	uint32_t inaccessable;
};

struct object_planet {
	int nresources;
	struct planet_resource *resources;
};

struct fleet_ship {
	uint32_t design;
	uint32_t count;
};

struct object_fleet {
	int nships;
	struct fleet_ship *ships;
	uint32_t damage;
};

/* Represents an object in the system */
struct object {
	uint32_t oid;	/* Unique ID */
	enum objtype type;
	char *name;

	uint32_t owner;

	uint64_t size;
	struct vector pos;
	struct vector vel;

	int nchildren;
	int *children;

	int nordertypes;
	int *ordertypes;

	int norders;

	uint64_t updated;

	struct object_fleet *fleet;
	struct object_planet *planet;

	struct tpe_gui_obj *gui;
};


struct tpe_obj * tpe_obj_init(struct tpe *tpe);
int tpe_obj_obj_dump(struct object *o);
struct object *tpe_obj_obj_get_by_id(struct tpe_obj *obj, int oid);
struct object *tpe_obj_obj_add(struct tpe_obj *obj, int );


