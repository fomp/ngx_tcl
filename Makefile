NGINX_PATH=/home/dino/nginx-1.4.4/
NGINX=$(NGINX_PATH)/objs/nginx
NGINX_MAKEFILE=$(NGINX_PATH)/Makefile

.PHONY: all build test

all: $(NGINX)

build: $(NGINX)

test: $(NGINX)
	$(NGINX) -p $(PWD)/test

$(NGINX): $(NGINX_MAKEFILE) ngx_tcl.c
	cd $(NGINX_PATH); make

$(NGINX_MAKEFILE): config
	cd $(NGINX_PATH); ./configure --add-module=$(PWD)

clean:
	cd $(NGINX_PATH); if [ -f Makefile ]; then make clean; fi
