compile

x86:
    ./premake.sh x86 0 true

nt966x:
    ./premake.sh nt966x 0 true

    ./premake.sh nt966x_d048 0 true

dep project:
    pjproject-2.7.2
    zlog (x86 platform)
