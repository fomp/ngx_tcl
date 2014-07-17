TCL_SRC=/home/dino/tcl8.5.15/
TCL_PATH=$(PWD)/tcl
TCL=tcl/tclsh
TCL_MAKEFILE=$(TCL_PATH)/Makefile

NGINX_PATH=/home/dino/nginx-1.6.0/
NGINX=$(NGINX_PATH)/objs/nginx
NGINX_MAKEFILE=$(NGINX_PATH)/Makefile

.PHONY: all build test

all: build

build: $(NGINX) $(TCL)

test: build
	pkill -9 nginx || true
	LD_LIBRARY_PATH=$(TCL_PATH) $(NGINX) -p $(PWD)/test

$(NGINX): $(NGINX_MAKEFILE) ngx_tcl.c
	cd $(NGINX_PATH); make

$(NGINX_MAKEFILE): config
	cd $(NGINX_PATH); ./configure --add-module=$(PWD)

$(TCL_MAKEFILE):
	[ -d $(TCL_PATH) ] || mkdir $(TCL_PATH)
	cd $(TCL_PATH); $(TCL_SRC)/unix/configure

$(TCL): $(TCL_MAKEFILE)
	[ -d $(TCL_PATH) ] || mkdir $(TCL_PATH)
	cd $(TCL_PATH); ln -sf libtcl85.so.1 libtcl85.so; make

clean:
	cd $(NGINX_PATH); if [ -f Makefile ]; then make clean; fi
	cd $(TCL_PATH); if [ -f Makefile ]; then make clean; fi
