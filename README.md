# aucont
Simple container (a-la docker, but very-very simplefied) written on linux namespaces and cgroups

## build

    ./build.sh

## utils

    $ ./aucont_start [--net <ip>] [--cpu <percentage>] [-d] /path/to/rootfs/ <command>
    5224

That command should start container with it's own pid, mount, net,... namespaces

    $ ./aucont_list 

You can run `aucont_list` to see ids of all running containers.

    $ ./aucont_exec <container_id> <command>

Run command inside container.

    $ ./aucont_stop <container_id> [<signal_num>]

Simple killing a container with given signal (SIGTERM by default)
