top = '.'
out = 'build'

kernel_sources = [
    'kernel/assert.c',
    'kernel/early-mmu.c',
    'kernel/init.c',
    'kernel/interrupts.c',
    'kernel/kmalloc.c',
    'kernel/large-object-cache.c',
    'kernel/message.c',
    'kernel/mmu.c',
    'kernel/object-cache.c',
    'kernel/once.c',
    'kernel/process.c',
    'kernel/procmgr.c',
    'kernel/procmgr_getpid.c',
    'kernel/procmgr_interrupts.c',
    'kernel/procmgr_map.c',
    'kernel/ramfs.c',
    'kernel/small-object-cache.c',
    'kernel/stdlib.c',
    'kernel/syscall.c',
    'kernel/thread.c',
    'kernel/timer.c',
    'kernel/tree-map.c',
    'kernel/vm.c',

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

    conf.setenv('native')

    conf.load('compiler_c')
    conf.load('compiler_cxx')

    #
    # Make enviornment to do all of the cross compiling for target system's
    # user programs and kernel.
    #

    conf.setenv('cross')

    # Suppress Waf from automatically inserting some linker flags based
    # on the build machine's OS
    conf.env.DEST_OS = ''

    conf.env.AS = 'arm-none-eabi-as'
    conf.env.AR = 'arm-none-eabi-ar'
    conf.env.CC = 'arm-none-eabi-gcc'
    conf.env.LD = 'arm-none-eabi-ld'

    conf.env.CFLAGS     = [ '-march=armv6', '-g', '-Wall', '-Werror' ]
    conf.env.ASFLAGS    = [ '-march=armv6', '-g' ]

    conf.load('gcc')
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
        source      =   [ 'libc/crt.c', 'libc/syscall.c', 'libc/user_io.c', 'libc/user_message.c', 'libc/user_process.c' ],
        target      =   'c',
        includes    =   [ 'include' ],
        env         =   ctx.all_envs['cross'].derive(),
    )

    for (p, src_list, link_base_addr) in user_progs:
        p_tgen = ctx.program(
            source      =   src_list,
            target      =   p,
            includes    =   [ 'include' ],
            linkflags   =   [ '-nostdlib', '-Wl,-Ttext-segment,0x%x' % link_base_addr ],
            use         =   'c',
            env         =   ctx.all_envs['cross'].derive(),
        )
        p_tgen.post()
        progs += [ p_tgen ]

    #
    # Tool to compile IFS
    #

    fs_builder = ctx.program(
        source      =   'fs-builder.cc',
        target      =   'fs-builder',
        env         =   ctx.all_envs['native'].derive(),
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

        env         =   ctx.all_envs['cross'].derive(),
    )
    image.post()

    # Be sure to rebuild if linker script changes
    ctx.add_manual_dependency(image.link_task.outputs[0], linker_script)
