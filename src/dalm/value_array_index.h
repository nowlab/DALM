#ifndef VALUE_ARRAY_INDEX_H_
#define VALUE_ARRAY_INDEX_H_

#include<map>

namespace DALM{
	typedef union {
		float val;
		uint32_t bits;
	} FloatBits;

	class ValueArrayIndex{
		public:
			ValueArrayIndex(ValueArray &ary){
				size_t len = ary.length();

				for(size_t i = 0; i < len; i++){
					FloatBits d;
					d.val = ary[i];
					value_index[d.bits] = i;
				}
			}

			size_t lookup(float value){
				FloatBits d;
				d.val = value;
				return value_index.find(d.bits)->second;
			}

		private:
			std::map<uint32_t, size_t> value_index;
	};
}

#endif // VALUE_ARRAY_INDEX_H_

