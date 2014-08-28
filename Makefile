TCL_PATH=/home/dino/tcl85
NGINX_PATH=/home/dino/src/nginx-1.6/

NGINX=$(NGINX_PATH)/objs/nginx
NGINX_MAKEFILE=$(NGINX_PATH)/Makefile
SRC=ngx_tcl.c ngx_tcl_var.c ngx_tcl_var.h ngx_tcl_header.c ngx_tcl_header.h

.PHONY: all build test

all: build

build: $(NGINX)

test: build
	LD_LIBRARY_PATH=$(TCL_PATH)/lib $(NGINX) -p $(PWD)/test

$(NGINX): $(NGINX_MAKEFILE) $(SRC)
	cd $(NGINX_PATH); make

$(NGINX_MAKEFILE): config
	cd $(NGINX_PATH); \
	TCL_PATH=$(TCL_PATH) \
	auto/configure --add-module=$(PWD) --with-http_ssl_module \
		--with-http_spdy_module

clean:
	cd $(NGINX_PATH); if [ -f Makefile ]; then make clean; fi
