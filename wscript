import platform
top='.'
out='.build'

def options(opt):
    opt.load('compiler_cxx')

def configure(conf):
    conf.load('compiler_cxx')
    
    # for debug
    #option = '-g'
    #option = '-g -pg'

    # normal optimization.
    #option = '-O3 -Wall -DNDEBUG'
    #option = '-O3 -Wall -DNDEBUG -g -pg'

    # same compile options as moses' kenlm(profile).
    #option = '-ftemplate-depth-128 -O3 -finline-functions -Wno-inline -Wall -g -pg -pthread  -DBOOST_FILE_SYSTEM_DYN_LINK -DBOOST_IOSTREAMS_DYN_LINK -DBOOST_PROGRAM_OPTIONS_DYN_LINK -DBOOST_SYSTEM_DYN_LINK -DBOOST_TEST_DYN_LINK -DBOOST_THREAD_DYN_DLL -DKENLM_MAX_ORDER=6 -DNDEBUG -DTRACE_ENABLE=1 -DWITH_THREADS -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES'
    # same compile options as moses' kenlm(release).
    option = '-ftemplate-depth-128 -O3 -finline-functions -Wno-inline -Wall -g -pthread -DKENLM_MAX_ORDER=6 -DNDEBUG -DTRACE_ENABLE=1 -DWITH_THREADS -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES'

	conf.env['LIB']=['pthread']
    	if platform.system() != "Darwin":
        	conf.env['LIB'].append('SegFault')
	conf.env['INCLUDES']=['darts-clone']

    conf.env['LIB']=['pthread', 'SegFault']
    #conf.env['LIB']=['pthread']
    conf.env['INCLUDES']=['externals']

    conf.define("DALM_MAX_ORDER",6)

def build(bld):
    projname = 'dalm'
    includes = ['include']
    use = [projname]

    ###### INSTALL HEADERS ######
    main_headers = ['src/dalm.h']
    headers = ['arpafile.h', 'buildutil.h', 'da.h', 'fileutil.h', 'fragment.h', 'handler.h', 'logger.h', 'pthread_wrapper.h', 'reversefile.h', 'sortutil.h', 'state.h', 'treefile.h', 'trie_reader.h', 'value_array.h', 'value_array_index.h', 'vocabulary.h', 'version.h', 'vocabulary_file.h']
    headers = map(lambda x:'src/dalm/%s'%x, headers)
    darts = ['externals/darts.h']
    bld.install_files('${PREFIX}/include', main_headers)
    bld.install_files('${PREFIX}/include/dalm', headers)
    bld.install_files('${PREFIX}/darts-clone', darts)

    ###### BUILD DALM LIB ######
    files = ['bst_da.cpp', 'build_util.cpp', 'dalm.cpp', 'embedded_da.cpp', 'fragment.cpp', 'logger.cpp', 'reverse_da.cpp', 'reverse_trie_da.cpp', 'vocabulary.cpp', 'value_array.cpp', 'vocabulary_file.cpp']
    files = map(lambda x: 'src/%s'%x, files)
    
    lib = bld.stlib(source=' '.join(files), target=projname)
    lib.includes = ['include']
    bld.install_files('${PREFIX}/lib/', ['lib%s.a'%projname])

    ###### BUILD DALM BUILDER ######
    files = ['builder.cpp']
    files = map(lambda x: 'src/%s'%x, files)
    builder = bld.program(source=' '.join(files), target='build_' + projname)
    builder.use = use
    builder.includes = includes

    ###### BUILD DALM CHECKER ######
    files = ['checker.cpp']
    files = map(lambda x: 'src/%s'%x, files)
    builder = bld.program(source=' '.join(files), target='validate_' + projname)
    builder.use = use
    builder.includes = includes

    ###### BUILD SAMPLES ######
    files = ['query_sample.cpp']
    files = map(lambda x: 'sample/%s'%x, files)
    builder = bld.program(source=' '.join(files), target='query_' + projname)
    builder.use = use
    builder.includes = includes

