# vim: set syntax=python:
from waflib import Configure
from waflib import Task
from waflib.Build import BuildContext
from waflib.TaskGen import after_method, before_method, feature

top = '.'
out = 'build'

Configure.autoconfig = True

CROSS   = 'cross'
NATIVE  = 'native'

kernel_sources = [
    'kernel/assert.cpp',
    'kernel/early-mmu.c',
    'kernel/debug.cpp',
    'kernel/debug-pl011.cpp',
    'kernel/exception.cpp',
    'kernel/init.cpp',
    'kernel/interrupts.cpp',
    'kernel/interrupts-pl190.cpp',
    'kernel/kmalloc.cpp',
    'kernel/large-object-cache.cpp',
    'kernel/message.cpp',
    'kernel/mmu.cpp',
    'kernel/nameserver.cpp',
    'kernel/object-cache.cpp',
    'kernel/once.cpp',
    'kernel/process.cpp',
    'kernel/procmgr.cpp',
    'kernel/procmgr_childwait.cpp',
    'kernel/procmgr_getpid.cpp',
    'kernel/procmgr_interrupts.cpp',
    'kernel/procmgr_map.cpp',
    'kernel/procmgr_naming.cpp',
    'kernel/procmgr_spawn.cpp',
    'kernel/ramfs.cpp',
    'kernel/reaper.cpp',
    'kernel/semaphore.cpp',
    'kernel/small-object-cache.cpp',
    'kernel/stdlib.c',
    'kernel/string.cpp',
    'kernel/syscall.cpp',
    'kernel/thread.cpp',
    'kernel/timer.cpp',
    'kernel/timer-sp804.cpp',
    'kernel/tree-map.cpp',
    'kernel/vm.cpp',

    'kernel/atomic.S',
    'kernel/early-entry.S',
    'kernel/exception-entry.S',
    'kernel/high-entry.S',
    'kernel/vector.S',

    'newlib/stubs.c',
]

libc_sources = [
    'libc/crt.c',
    'libc/syscall.c',
    'libc/user_io.c',
    'libc/user_message.c',
    'libc/user_naming.c',
    'libc/user_process.c',
    'newlib/stubs.c',
]

user_progs = [
    ('echo',            ['echo.c'],             0x10000),
    ('echo-client',     ['echo-client.c'],      0x20000),
    ('uio',             ['uio.c'],              0x30000),
    ('pl011',           ['pl011.c'],            0x40000),
    ('crasher',         ['crasher.c'],          0x50000),
    ('init',            ['init.c'],             0x60000),
]

def options(opt):
    opt.load('compiler_c')
    opt.load('compiler_cxx')
    opt.load('asm')

def configure(conf):

    conf.load('doxygen')

    #
    # Make environment suitable for compiling custom programs that are
    # used to preprocess artifacts for inclusion into the target system.
    #

    conf.setenv(NATIVE)

    conf.load('compiler_c')
    conf.load('compiler_cxx')

    #
    # Make enviornment to do all of the cross compiling for target system's
    # user programs and kernel.
    #

    conf.setenv(CROSS)

    conf.find_program('arm-none-eabi-as', var='AS')
    conf.find_program('arm-none-eabi-ar', var='AR')
    conf.find_program('arm-none-eabi-gcc', var='CC')
    conf.find_program('arm-none-eabi-g++', var='CXX')
    conf.find_program('arm-none-eabi-ld', var='LD')

    cflags = ['-march=armv6', '-g', '-Wall', '-Werror']
    asflags = ['-march=armv6', '-g']

    conf.env.append_unique('CFLAGS', cflags)
    conf.env.append_unique('CXXFLAGS', cflags)
    conf.env.append_unique('ASFLAGS', asflags)

    conf.load('gcc')
    conf.load('gxx')
    conf.load('gas')

    # Override defaults from Waf's GCC support
    conf.env.SHLIB_MARKER = ''
    conf.env.STLIB_MARKER = ''

    conf.load('compiler_c')
    conf.load('compiler_cxx')
    conf.load('asm')

def build(bld):

    #
    # Unprivileged user programs
    #

    bld.add_group()

    bld(source    =   libc_sources,
        target    =   'my_c',
        includes  =   ['include'],
        features  =   ['c'],
        env       =   bld.all_envs[CROSS].derive())

    for (p, src_list, link_base_addr) in user_progs:
        bld.program(source      = src_list,
                    target      = p,
                    includes    = ['include'],
                    linkflags   = ['-nostartfiles', '-Wl,-Ttext-segment,0x%x' % link_base_addr],
                    use         = 'my_c',
                    env         = bld.all_envs[CROSS].derive())

    #
    # Tool to compile IFS
    #

    bld.program(source  = 'fs-builder.cc',
                target  = 'fs-builder',
                env     = bld.all_envs[NATIVE].derive())

    #
    # Generate source code of IFS
    #

    bld.add_group()

    bld(target      = 'ramfs_image.c',
        features    = ['fs-builder'],
        progs       = [up[0] for up in user_progs])


    #
    # Main kernel image
    #

    bld.add_group()

    image = bld.program(source          = kernel_sources,
                        target          = 'image',
                        includes        = ['include'],

                        defines         = ['__KERNEL__'],

                        linkflags       = ['-nostartfiles'],

                        env             = bld.all_envs[CROSS].derive(),

                        features        = ['asmoffsets', 'ramfs_source', 'ldscript', 'linkermap'],
                        asmoffsets      = 'asm-offsets.c',
                        ramfs_source    = 'ramfs_image.c',
                        ldscript        = 'kernel/kernel.ldscript')

"""
Look up the linked executable for each of the taskgens
named in taskgen.progs, and generate a RAM filesystem
containing all of them.
"""
@feature('fs-builder')
def discover_fs_builder_inputs(taskgen):
    fsbuilder_link_task = taskgen.bld.get_tgen_by_name('fs-builder').link_task
    prognodes = [taskgen.bld.get_tgen_by_name(p).link_task.outputs[0] for p in taskgen.progs]
    target = taskgen.path.find_or_declare(taskgen.target)

    # Create task, set up its environment
    task = taskgen.create_task('fs_builder', prognodes, target)
    task.env.FS_BUILDER = fsbuilder_link_task.outputs[0].abspath()

    # Patch up dependencies
    task.set_run_after(fsbuilder_link_task)
    taskgen.bld.add_manual_dependency(target, fsbuilder_link_task.outputs[0])

    # Save reference to the task
    taskgen.fs_builder_task = task

class fs_builder(Task.Task):
    run_str = '${FS_BUILDER} -o ${TGT} -n RamFsImage ${SRC}'

"""
Append the source emitted by a ram filesystem builder, to the inputs
of a regular C/C++ compiled program
"""
@feature('ramfs_source')
@before_method('process_source')
def add_ramfs_sources(taskgen):
    built_source = taskgen.bld.get_tgen_by_name(taskgen.ramfs_source).fs_builder_task.outputs[0]
    taskgen.source.append(built_source)

"""
Add a given linker script to the LINKFLAGS and update dependencies so that
the binary will re-link if the script changes.
"""
@feature('ldscript')
@after_method('apply_link')
@before_method('propagate_uselib_vars')
def apply_ldscript(taskgen):
    script = taskgen.path.find_or_declare(taskgen.ldscript)
    taskgen.bld.add_manual_dependency(taskgen.link_task.outputs[0], script)
    taskgen.linkflags += ['-Wl,-T,%s' % script.bldpath()]

"""
Tack on linker flags to cause a linker map to be generated
"""
@feature('linkermap')
@after_method('apply_link')
@before_method('propagate_uselib_vars')
def apply_linkermap(taskgen):
    mapfile = taskgen.link_task.outputs[0].change_ext('.map')
    taskgen.link_task.outputs.append(mapfile)
    taskgen.linkflags += ['-Wl,-Map,%s' % mapfile.bldpath()]

"""
After process_source(), so that the compiled_tasks[] will be populated already.

Before propagate_uselib_vars(), so that changes to includes[] will still be
incorporated into the finished INCLUDES environment.
"""
@feature('asmoffsets')
@after_method('process_source')
@before_method('propagate_uselib_vars')
def apply_asmoffsets(taskgen):
    input       = taskgen.path.find_resource(taskgen.asmoffsets)
    input_ext   = input.name[input.name.rfind('.'):]
    asm         = input.change_ext('%s.%d.s' % (input_ext, taskgen.idx))
    header      = input.change_ext('.h')

    cxx_task = taskgen.create_task('cxx', input, asm)
    cxx_task.env.CXX_TGT_F = ['-S', '-o']
    cxx_task.env.append_value('CXXFLAGS', '-Wno-invalid-offsetof')

    grep_task = taskgen.create_task('asmoffsets', asm, header)
    taskgen.includes.append(header.parent)

    # Force all our #include-using ASM tasks to run after this
    asmclass = Task.classes['asm']
    for asmtask in [t for t in taskgen.compiled_tasks if isinstance(t, asmclass)]:
        taskgen.bld.add_manual_dependency(asmtask.outputs[0], header)
        asmtask.set_run_after(grep_task)

    # Put the directory holding the generated header into the search path
    taskgen.includes.append(header.parent)

class asmoffsets(Task.Task):

    def run(self):
        outlines = []

        outlines.append('#ifndef __OFFSETS_H__')
        outlines.append('#define __OFFSETS_H__')
        outlines.append('')

        for line in self.inputs[0].read().splitlines():
            if line.startswith('#define'):
                outlines.append(line)

        outlines.append('')
        outlines.append('#endif /* __OFFSETS_H__ */')

        self.outputs[0].write('\n'.join(outlines))
        return 0

# Force main 'asm' to be loaded so that we can be sure that our
# custom hook for .S files get installed after asm's
from waflib.Tools import asm
from waflib.TaskGen import extension
@extension('.S')
def asm_hook(taskgen, node):
    preproc = node.parent.find_or_declare('%s.%d.i' % (node.name, taskgen.idx))
    t = taskgen.create_task('c', node, preproc)
    t.env['CC_TGT_F'] = ['-E', '-o']
    return taskgen.create_compiled_task('asm', preproc)

def doxygen(bld):
    bld(features = ['doxygen'], doxyfile = 'Doxyfile')

class DoxygenContext(BuildContext):
    fun = 'doxygen'
    cmd = 'doxygen'

import waflib.extras.doxygen
Task.always_run(waflib.extras.doxygen.doxygen)
