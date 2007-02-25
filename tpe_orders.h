struct order_arg {
	char *name;
	uint32_t arg_type;
	char *description;
};

struct arg_type6 {
	int id;
	char *name;
	int max;
};
struct tpe_orders;
struct tpe_orders *tpe_orders_init(struct tpe *tpe);
int tpe_order_get_type_by_name(struct tpe *tpe, const char *name);