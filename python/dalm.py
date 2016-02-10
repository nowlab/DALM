from _dalm import State, LM, Vocabulary, Model, _build

class Builder(object):
    bst = "bst"
    reverse = "reverse"

    def __init__(self, memory_limit=1024, n_cores=1):
        self.memory_limit = memory_limit
        self.n_cores = n_cores

    def __call__(self, output, arpa, opt="reverse", n_div=1):
        _build(output, arpa, opt, n_div, self.n_cores, self.memory_limit)

