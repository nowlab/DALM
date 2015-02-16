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
	option = '-O3 -Wall -DNDEBUG -g'
	#option = '-O3 -Wall -DNDEBUG -g -pg'

	conf.env['CFLAGS']=option.split(" ")
	conf.env['CXXFLAGS']=option.split(" ")

	conf.env['LIB']=['pthread', 'SegFault']
	conf.env['INCLUDES']=['darts-clone']

	conf.define("DALM_MAX_ORDER",6)

def build(bld):
	bld.recurse('scripts')
	
	projname = 'dalm'
	includes = ['include']
	use = [projname]

	###### INSTALL HEADERS ######
	headers = ['arpafile.h', 'da.h', 'dalm.h', 'fragment.h', 'handler.h', 'logger.h', 'pthread_wrapper.h', 'state.h', 'treefile.h', 'value_array.h', 'value_array_index.h', 'vocabulary.h', 'version.h']
	headers = map(lambda x:'include/%s'%x, headers)
	bld.install_files('${PREFIX}/include', headers)

	headers = ['darts.h']
	headers = map(lambda x:'darts-clone/%s'%x, headers)
	bld.install_files('${PREFIX}/darts-clone', headers)

	###### BUILD DALM LIB ######
	files = ['embedded_da.cpp', 'lm.cpp', 'fragment.cpp', 'logger.cpp', 'vocabulary.cpp', 'value_array.cpp']
	files = map(lambda x: 'src/%s'%x, files)
	
	lib = bld.stlib(source=' '.join(files), target=projname)
	lib.includes = ['include']
	bld.install_files('${PREFIX}/lib/', ['lib%s.a'%projname])

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
