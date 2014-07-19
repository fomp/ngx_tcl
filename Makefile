TCL_PATH=/home/dino/tcl85
NGINX_PATH=/home/dino/nginx-1.6.0/
NGINX=$(NGINX_PATH)/objs/nginx
NGINX_MAKEFILE=$(NGINX_PATH)/Makefile

.PHONY: all build test

all: build

build: $(NGINX)

test: build
	LD_LIBRARY_PATH=$(TCL_PATH)/lib $(NGINX) -p $(PWD)/test

$(NGINX): $(NGINX_MAKEFILE) ngx_tcl.c
	cd $(NGINX_PATH); make

$(NGINX_MAKEFILE): config
	cd $(NGINX_PATH); \
	TCL_PATH=$(TCL_PATH) \
	./configure --add-module=$(PWD)

clean:
	cd $(NGINX_PATH); if [ -f Makefile ]; then make clean; fi
