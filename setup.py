from distutils.core import setup, Extension

module1 = Extension('acc_monitor',
                    sources = ['acc_monitor.c', 'tcp_transfer.c', 'rdma_server.c'],
		    include_dirs = ['.'],
		    libraries = [
                'rdmacm',
                'pthread',
                'ibverbs',
                'fpga'],
                    library_dirs = ['/usr/lib', '/home/900/lxs900/zzd/testAPI/fpga-sim/driver'] )
setup (name = 'PackageName',
       version = '1.0',
       description = 'This is a fpga server package',
       ext_modules = [module1])

