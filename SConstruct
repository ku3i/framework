import os

env_options = {
    "CC"    : "gcc",
    "CXX"   : "g++",
    "LD"    : "g++",
    "AR"    : "ar",
    "STRIP" : "strip",
}

env = Environment(**env_options)
# assuming your cross compiler bin dir is in your PATH variable
env.Append(ENV = {'PATH' : os.environ['PATH']})
Export('env')

env.SConscript('src/SConscript', variant_dir='build', duplicate=0)
