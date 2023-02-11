all:
	gcc -o mnc -l pthread mnc.c
	cp mnc proxy_tcp_demo/v_cli
	cp mnc proxy_tcp_demo/v_serv
	find -type f -name '*.sh' -exec chmod +x {} \;
