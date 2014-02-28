top='.'
out='.build'

def options(opt):
	opt.load('compiler_cxx')

def configure(conf):
	conf.load('compiler_cxx')
	conf.env['CFLAGS']=['-O3', '-Wall', '-DNDEBUG']
	conf.env['CXXFLAGS']=['-O3', '-Wall', '-DNDEBUG']
	conf.env['LIB']=['pthread']
	conf.env['INCLUDES']=['darts-clone']

def build(bld):
	bld.recurse('scripts')
	
	projname = 'dalm'
	includes = ['include']
	use = [projname]

	###### INSTALL HEADERS ######
	headers = ['arpafile.h', 'da.h', 'dalm.h', 'fragment.h', 'logger.h', 'pthread_wrapper.h', 'state.h', 'treefile.h', 'value_array.h', 'value_array_index.h', 'vocabulary.h', 'version.h']
	headers = map(lambda x:'include/%s'%x, headers)
	bld.install_files('${PREFIX}/include', headers)

	headers = ['darts.h']
	headers = map(lambda x:'darts-clone/%s'%x, headers)
	bld.install_files('${PREFIX}/darts-clone', headers)

	###### BUILD DALM LIB ######
	files = ['lm.cpp', 'da.cpp', 'fragment.cpp', 'vocabulary.cpp', 'value_array.cpp', 'logger.cpp']
	files = map(lambda x: 'src/%s'%x, files)
	
	lib = bld.stlib(source=' '.join(files), target=projname)
	lib.includes = ['include']
	bld.install_files('${PREFIX}/lib/', ['lib%s.a'%projname])

	###### BUILD DUMPER ######
	files = ['dumper.cpp']
	files = map(lambda x: 'src/%s'%x, files)
	
	dumper = bld.program(source=' '.join(files), target='trie_dumper')
	dumper.use = use
	dumper.includes = includes

	###### BUILD DALM BUILDER ######
	files = ['builder.cpp']
	files = map(lambda x: 'src/%s'%x, files)
	builder = bld.program(source=' '.join(files), target=projname+'_builder')
	builder.use = use
	builder.includes = includes

	###### BUILD SAMPLES ######
	files = ['query_sample.cpp']
	files = map(lambda x: 'sample/%s'%x, files)
	builder = bld.program(source=' '.join(files), target='query_sample')
	builder.use = use
	builder.includes = includes

	files = ['query_with_state_sample.cpp']
	files = map(lambda x: 'sample/%s'%x, files)
	builder = bld.program(source=' '.join(files), target='query_with_state_sample')
	builder.use = use
	builder.includes = includes
