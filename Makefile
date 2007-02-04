PKGCONFIG=pkg-config
PKGS='evas ecore edje'

CFLAGS+=`${PKGCONFIG} --cflags ${PKGS}`
LDFLAGS+=`${PKGCONFIG} --libs ${PKGS}`

.TARGETS: tpe ${EDJE}

OBJECTS=	\
	tpe.o	\
	tpe_comm.o	\
	tpe_event.o	\
	tpe_gui.o	\
	tpe_msg.o

EDJE=	edje/basic.edj

tpe: ${OBJECTS} 
edje/basic.edj : edje/basic.edc
	edje_cc edje/basic.edc

tpe_msg.o : tpe_msg.h tpe.h

