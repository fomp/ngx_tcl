NGINX_PATH=/home/dino/nginx-1.4.4/

all: build

build:
	cd $(NGINX_PATH); make

configure: config
	cd $(NGINX_PATH); ./configure --add-module=$(PWD)

clean:
	cd $(NGINX_PATH); make clean
