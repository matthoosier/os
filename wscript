top = '.'
out = 'build'

CROSS   = 'cross'
NATIVE  = 'native'

kernel_sources = [
    'kernel/assert.cpp',
    'kernel/early-mmu.cpp',
    'kernel/init.cpp',
    'kernel/interrupts.cpp',
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
    'kernel/small-object-cache.cpp',
    'kernel/stdlib.c',
    'kernel/syscall.c',
    'kernel/thread.cpp',
    'kernel/timer.cpp',
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

    # Suppress Waf from automatically inserting some linker flags based
    # on the build machine's OS
    conf.env.DEST_OS = ''

    conf.find_program('arm-none-eabi-as', var='AS')
    conf.find_program('arm-none-eabi-ar', var='AR')
    conf.find_program('arm-none-eabi-gcc', var='CC')
    conf.find_program('arm-none-eabi-g++', var='CXX')
    conf.find_program('arm-none-eabi-ld', var='LD')

    cflags = [ '-march=armv6', '-g', '-Wall', '-Werror' ]
    asflags = [ '-march=armv6', '-g' ]

    conf.env.append_unique('CFLAGS', cflags)
    conf.env.append_unique('CXXFLAGS', cflags + ['-fno-exceptions'])
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

def build(ctx):

    #
    # Unprivileged user programs
    #

    progs = []

    libc = ctx.stlib(
        source      =   libc_sources,
        target      =   'my_c',
        includes    =   [ 'include' ],
        env         =   ctx.all_envs[CROSS].derive(),
    )

    for (p, src_list, link_base_addr) in user_progs:
        p_tgen = ctx.program(
            source      =   src_list,
            target      =   p,
            includes    =   [ 'include' ],
            linkflags   =   [ '-nostartfiles', '-Wl,-Ttext-segment,0x%x' % link_base_addr ],
            use         =   'my_c',
            env         =   ctx.all_envs[CROSS].derive(),
        )
        p_tgen.post()
        progs += [ p_tgen ]

    #
    # Tool to compile IFS
    #

    fs_builder = ctx.program(
        source      =   'fs-builder.cc',
        target      =   'fs-builder',
        env         =   ctx.all_envs[NATIVE].derive(),
    )

    fs_builder.post()
    fs_builder_bin = fs_builder.link_task.outputs[0]

    #
    # Generate source code of IFS
    #

    ctx.env.FS_BUILDER = fs_builder_bin.abspath()

    ramfs_image_c = ctx(
            rule    =   '${FS_BUILDER} -o ${TGT} -n RamFsImage ' + ' '.join([ ('"%s"' % p.link_task.outputs[0].bldpath()) for p in progs ]),
            target  =   'ramfs_image.c',
            source  =   [ fs_builder_bin ] + [ p.link_task.outputs[0].bldpath() for p in progs ],
    )
    ramfs_image_c.post()

    # Assume that this custom taskgen only has one task
    if len(ramfs_image_c.tasks) != 1:
        ctx.fatal("Can't deal with RamFsBuilder rule that makes more than 1 task")

    # ... and that it has only one output
    if len(ramfs_image_c.tasks[0].outputs) != 1:
        ctx.fatal("Can't deal with RamFsBuilder rule that makes more than 1 output")


    #
    # Main kernel image
    #

    linker_script = ctx.path.find_resource('kernel.ldscript')

    image = ctx.program(
        source      =   kernel_sources + [ ramfs_image_c.tasks[0].outputs[0] ],
        target      =   'image',
        includes    =   [ 'include', '.' ],

        defines     =   [ '__KERNEL__' ],

        linkflags   =   [ '-Wl,-T,' + linker_script.bldpath(), '-nostdlib' ],

        env         =   ctx.all_envs[CROSS].derive(),
    )
    image.post()

    # Be sure to rebuild if linker script changes
    ctx.add_manual_dependency(image.link_task.outputs[0], linker_script)
