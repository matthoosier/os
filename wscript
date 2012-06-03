from waflib import Task
from waflib.Build import BuildContext
from waflib.TaskGen import after_method, before_method, feature

top = '.'
out = 'build'

CROSS   = 'cross'
NATIVE  = 'native'

kernel_sources = [
    'kernel/assert.cpp',
    'kernel/early-mmu.c',
    'kernel/init.cpp',
    'kernel/interrupts.cpp',
    'kernel/interrupts-pl190.cpp',
    'kernel/kmalloc.cpp',
    'kernel/large-object-cache.cpp',
    'kernel/message.cpp',
    'kernel/mmu.cpp',
    'kernel/object-cache.cpp',
    'kernel/once.cpp',
    'kernel/process.cpp',
    'kernel/procmgr.cpp',
    'kernel/procmgr_getpid.cpp',
    'kernel/procmgr_interrupts.cpp',
    'kernel/procmgr_map.cpp',
    'kernel/ramfs.cpp',
    'kernel/semaphore.cpp',
    'kernel/small-object-cache.cpp',
    'kernel/stdlib.c',
    'kernel/syscall.cpp',
    'kernel/thread.cpp',
    'kernel/timer.cpp',
    'kernel/timer-sp804.cpp',
    'kernel/tree-map.cpp',
    'kernel/vm.cpp',

    'kernel/atomic.S',
    'kernel/early-entry.S',
    'kernel/high-entry.S',
    'kernel/vector.S',
]

libc_sources = [
    'libc/crt.c',
    'libc/syscall.c',
    'libc/user_io.c',
    'libc/user_message.c',
    'libc/user_process.c',
]

user_progs = [
    ('echo',            [ 'echo.c' ],           0x10000),
    ('syscall-client',  [ 'syscall-client.c' ], 0x20000),
    ('uio',             [ 'uio.c' ],            0x30000),
    ('pl011',           [ 'pl011.c' ],          0x40000),
]

def options(opt):
    opt.load('compiler_c')
    opt.load('compiler_cxx')
    opt.load('asm')

def configure(conf):

    conf.find_program('doxygen')

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

    conf.find_program('arm-none-eabi-gcc', var='AS')
    conf.find_program('arm-none-eabi-ar', var='AR')
    conf.find_program('arm-none-eabi-gcc', var='CC')
    conf.find_program('arm-none-eabi-g++', var='CXX')
    conf.find_program('arm-none-eabi-ld', var='LD')

    cflags = [ '-march=armv6', '-g', '-Wall', '-Werror' ]
    asflags = [ '-march=armv6', '-g' ]

    conf.env.append_unique('CFLAGS', cflags)
    conf.env.append_unique('CXXFLAGS', cflags)
    conf.env.append_unique('ASFLAGS', asflags)

    conf.load('gcc')
    conf.load('gxx')
    conf.load('gas')

    # Because we're using 'gcc' rather than 'gas' as the entrypoint
    # program to the assembler, we need to tell GCC to stop after
    # building the object code (don't link).
    conf.env.AS_TGT_F = ['-c', '-o']

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

    progs = []

    libc = bld.stlib(
        source      =   libc_sources,
        target      =   'my_c',
        includes    =   [ 'include' ],
        env         =   bld.all_envs[CROSS].derive(),
    )

    for (p, src_list, link_base_addr) in user_progs:
        p_tgen = bld.program(
            source      =   src_list,
            target      =   p,
            includes    =   [ 'include' ],
            linkflags   =   [ '-nostartfiles', '-Wl,-Ttext-segment,0x%x' % link_base_addr ],
            use         =   'my_c',
            env         =   bld.all_envs[CROSS].derive(),
        )
        p_tgen.post()
        progs += [ p_tgen ]

    #
    # Tool to compile IFS
    #

    fs_builder = bld.program(
        source      =   'fs-builder.cc',
        target      =   'fs-builder',
        env         =   bld.all_envs[NATIVE].derive(),
    )

    fs_builder.post()
    fs_builder_bin = fs_builder.link_task.outputs[0]

    #
    # Generate source code of IFS
    #

    bld.env.FS_BUILDER = fs_builder_bin.abspath()

    ramfs_image_c = bld(
            rule    =   '${FS_BUILDER} -o ${TGT} -n RamFsImage ' + ' '.join([ ('"%s"' % p.link_task.outputs[0].bldpath()) for p in progs ]),
            target  =   'ramfs_image.c',
            source  =   [ fs_builder_bin ] + [ p.link_task.outputs[0].bldpath() for p in progs ],
    )
    ramfs_image_c.post()

    # Assume that this custom taskgen only has one task
    if len(ramfs_image_c.tasks) != 1:
        bld.fatal("Can't deal with RamFsBuilder rule that makes more than 1 task")

    # ... and that it has only one output
    if len(ramfs_image_c.tasks[0].outputs) != 1:
        bld.fatal("Can't deal with RamFsBuilder rule that makes more than 1 output")


    #
    # Main kernel image
    #

    image = bld.program(
        source      =   kernel_sources + [ ramfs_image_c.tasks[0].outputs[0] ],
        target      =   'image',
        includes    =   [ 'include' ],

        defines     =   [ '__KERNEL__' ],

        linkflags   =   [ '-nostartfiles' ],

        env         =   bld.all_envs[CROSS].derive(),

        features    =   [ 'asmoffsets', 'ldscript' ],
        asmoffsets  =   'asm-offsets.c',
        ldscript    =   'kernel.ldscript',
    )

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
    taskgen.linkflags += [ '-Wl,-T,%s' % script.bldpath() ]

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

    grep_task = taskgen.create_task('asmoffsets', asm, header)
    taskgen.includes.append(header.parent)

    # Force all our #include-using ASM tasks to run after this
    asmclass = Task.Task.classes['asm']
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

def doc(bld):
    bld(features = 'doxygen')

@feature('doxygen')
def apply_doxygen(taskgen):
    taskgen.create_task('doxygen', taskgen.bld.path.find_resource('Doxyfile'), None)

class doxygen(Task.Task):
    run_str2 = '${DOXYGEN} ${SRC}'
    def run(self):
        self.exec_command(
            [self.env.DOXYGEN, self.inputs[0].srcpath()],
            cwd=self.generator.bld.path.abspath()           # src dir
        )

Task.always_run(doxygen)

class DocsContext(BuildContext):
    fun = 'doc'
    cmd = 'doc'
