#ifndef VALUE_ARRAY_INDEX_H_
#define VALUE_ARRAY_INDEX_H_

#include<map>

namespace DALM{
	class ValueArrayIndex{
		public:
			ValueArrayIndex(ValueArray &ary){
				size_t len = ary.length();

				for(size_t i = 0; i < len; i++){
					value_index[ary[i]] = i;
				}
			}

			size_t lookup(float value){
				return value_index.find(value)->second;
			}

		private:
			std::map<float, size_t> value_index;
	};
}

#endif // VALUE_ARRAY_INDEX_H_

