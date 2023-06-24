# user mode ko
linux user mode ko, like the ko module for kernel, but implement in user mode.
it can load .o(Relocatable Object File), relocate the object and run the entry(such as _start, APP_Root).

# git clone
used the flowing command to clone project to local host.
```
git clone --recursive https://github.com/shenyongge/UMKO.git
or
git clone https://github.com/shenyongge/UMKO.git
git submodule update --init --recursive
```
use the flowing command to add a submodule.
```
git submodule add <remote-url> <local-path>
```

# make and run
* `make aarch64` will build the project and the examples for aarch64.
* `make run_app00_aarch64` will run the app00 in aarch64 with qemu user mode.
* `make run_app10_aarch64` will run the app10 in aarch64 with qemu user mode.
* ![app10 run in aarch64 with qemu user mode](/images/run_app10_in_aarch64.png)

* `make x86_64` will build the project and the examples for x86_64.
* `make run_app00_aarch64` will run the app00 in x86_64.
* ![app00 run in x86_64](/images/run_app00_in_x86.png)

* `make all` will do all above.
* `make clean` will clean rm the build dir, and clean all temp files.