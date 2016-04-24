.PHONY: clean

APPS    := accServicelayer.so acc_monitor.so fpga-benchmark

CLIENT_CFLAGS := -I/usr/include
CLIENT_LIBS := -L/usr/lib/x86_64-linux-gnu
RDMA_LIBS := -lrdmacm -libverbs -lpthread
CFLAGS  := -Wall -g -fPIC

accServicelayer.so: acc_servicelayer.c tcp_transfer.c rdma_client.c fpga-sim/driver/fpga-libacc.c 
	$(CC) $(CFLAGS) -c -fpic acc_servicelayer.c tcp_transfer.c rdma_client.c fpga-sim/driver/fpga-libacc.c $(CLIENT_CFLAGS) $(CLIENT_LIBS) $(RDMA_LIBS)
	$(CC) $(CFLAGS) -shared acc_servicelayer.o tcp_transfer.o rdma_client.c fpga-libacc.o -o libaccServicelayer.so  $(RDMA_LIBS)
	sudo cp libaccServicelayer.so /lib64/
	sudo cp libaccServicelayer.so /lib/
	sudo cp libaccServicelayer.so /usr/local/lib/

fpga-benchmark: fpga-benchmark.c
	$(CC) $(CFLAGS) -o fpga-benchmark fpga-benchmark.c -laccServicelayer

acc_monitor.so: acc_monitor.c 
	python setup.py build 
	cp build/lib.linux-x86_64-2.7/acc_monitor.so .

all: ${APPS}

clean:
	rm -f ${APPS}
	rm -rf *.o
	rm -rf *.so
	sudo rm -rf build
