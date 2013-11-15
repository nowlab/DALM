#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits.h>

#include <utility>

#include <treefile.h>
#include <da.h>

#include <MurmurHash3.h>

using namespace DALM;

DA::DA(size_t daid, size_t datotal, DA **neighbours, Logger &logger)
	: daid(daid), datotal(datotal), da(neighbours), logger(logger){
		array_size=3;
		base_array = new _base[array_size];
		check_array = new int[array_size];
		value_id = new int[array_size];

		max_index=0;
		first_empty_index=1;

		for(unsigned i=0;i<array_size;i++){
			base_array[i].base_val = -1;
			check_array[i] = -1;
			value_id[i] = -1;
		}  
	}

DA::DA(FILE *fp, DA **neighbours, Logger &logger): logger(logger){
	fread(&daid,sizeof(unsigned),1,fp);
	fread(&datotal,sizeof(unsigned),1,fp);
	da = neighbours;
	fread(&max_index,sizeof(unsigned),1,fp);
	array_size = max_index+1;

	base_array = new _base[array_size];
	check_array = new int[array_size];

	fread(base_array,sizeof(_base),array_size,fp);
	fread(check_array,sizeof(int),array_size,fp);

	fread(&varray_size,sizeof(unsigned),1,fp);
	value_array = new float[varray_size];
	fread(value_array,sizeof(float),varray_size,fp);
	value_id = NULL;
	value_table = NULL;
	first_empty_index=0;
	logger << "DA[" << daid << "] da-size: " << array_size << Logger::endi;
	logger << "DA[" << daid << "] val-size: " << varray_size << Logger::endi;
}

DA::~DA(){
	if(base_array != NULL) delete [] base_array;
	if(check_array != NULL) delete [] check_array;
	if(value_id != NULL) delete [] value_id;
	if(value_array != NULL) delete [] value_array;
	if(value_table != NULL) delete [] value_table;
}

void DA::make_da(TreeFile &tf, unsigned unigram_type)
{
	resize_array(unigram_type*20);
	base_array[0].base_val=0;

	size_t total = tf.get_totalsize();
	logger << "DA[" << daid << "] total=" << total << Logger::endi;
	size_t order = tf.get_ngramorder();

	int *words = new int[unigram_type+1];
	float *values = new float[unigram_type+1];
	int *history = new int[order-1];
	size_t wordssize = 0;
	size_t historysize = 0;
	size_t terminal_pos=(size_t)-1;

	memset(history, 0, sizeof(int)*(order-1));

	size_t tenpercent = total / 10;
	for(size_t i = 0; i < total; i++){
		if((i+1) % tenpercent == 0){
			logger << "DA[" << daid << "] " << (i+1)/tenpercent << "0% done." << Logger::endi;
		}

		unsigned short n;
		VocabId *ngram;
		float value;
		tf.get_ngram(n,ngram,value);

		if(ngram[n-1]==0) ngram[n-1]=1;
		if(n>1 && ngram[n-2]==0) ngram[n-2]=1;

		if(n==2 && ngram[0]==1 && ngram[1]%datotal!=daid){
			delete [] ngram;
			continue;
		}else if(ngram[0]%datotal!=daid && ngram[0]!=1){
			delete [] ngram;
			continue;
		}

		if(historysize != (size_t)n-1
				|| memcmp(history, ngram, sizeof(int)*(n-1))!=0){

			unsigned now=0;
			for(size_t j = 0; j < historysize; j++){
				now = get_pos(history[j], now);
			}
			if(historysize!=0 && history[historysize-1]==1){
				det_base(words, values, wordssize, now);
			}else{
				det_base(words, NULL, wordssize, now);
				if(terminal_pos!=(size_t)-1){
					unsigned terminal=get_terminal(now);
					value_id[terminal] = value_find(values[terminal_pos]);
				}
			}

			memcpy(history, ngram, sizeof(int)*(n-1));
			historysize = n-1;
			wordssize=0;
			terminal_pos=(size_t)-1;
		}

		if(ngram[n-1]==1){
			terminal_pos=wordssize;
		}

		words[wordssize]=ngram[n-1];
		values[wordssize]=value;
		wordssize++;

		delete [] ngram;
	}
	unsigned now=0;
	for(size_t j = 0; j < historysize; j++){
		unsigned now_tmp = get_pos(history[j], now);
		now = now_tmp;
	}
	if(historysize!=0 && history[historysize-1]==1){
		det_base(words, values, wordssize, now);
	}else{
		det_base(words, NULL, wordssize, now);
		if(terminal_pos!=(size_t)-1){
			unsigned terminal=get_terminal(now);
			value_id[terminal] = value_find(values[terminal_pos]);
		}
	}

	value_replace();
	delete [] history;
	delete [] words;
	delete [] values;
}

void DA::dump(FILE *fp)
{
	fwrite(&daid,sizeof(unsigned),1,fp);
	fwrite(&datotal,sizeof(unsigned),1,fp);
	fwrite(&max_index,sizeof(unsigned),1,fp);
	fwrite(base_array,sizeof(_base),max_index+1,fp);
	fwrite(check_array,sizeof(int),max_index+1,fp);

	fwrite(&varray_size,sizeof(unsigned),1,fp);
	fwrite(value_array,sizeof(float),varray_size,fp);
	logger << "DA[" << daid << "] da-size: " << max_index << Logger::endi;
	logger << "DA[" << daid << "] val-size: " << varray_size << Logger::endi;
}

float DA::get_prob(int *word,int order){
	int length=0;
	float tmp_bow = 0.0;
	float tmp_prob = 0.0;

	int unigram_daid = word[0]%datotal;
	int unigram_terminal = da[unigram_daid]->get_terminal(0);
	int unigram_prob_pos = da[unigram_daid]->get_pos(word[0], unigram_terminal);
	if(unigram_prob_pos > 0){
		tmp_prob = da[unigram_daid]->base_array[unigram_prob_pos].logprob;
	}

	if(order > 0){
		int current_pos = get_pos(word[1], 0);
		length=1;

		while(current_pos > 0 && length <= order){
			int terminal = get_terminal(current_pos);
			tmp_bow += value_array[-check_array[terminal]];
			int prob_pos = get_pos(word[0], terminal);
			if(prob_pos > 0){
				tmp_prob = base_array[prob_pos].logprob;
				tmp_bow = 0.0;
			}

			if(length+1 <= order){
				int next_pos = get_pos(word[length+1], current_pos);
				if(next_pos > 0){
					current_pos = next_pos;
					length++;
				}else{
					break;
				}
			}else{
				break;
			}
		}
		return tmp_prob+tmp_bow;
	}else{
		return tmp_prob;
	}
}

unsigned long int DA::get_state(int *word,int order){
	if(order > 0){
		int length=0;
		int current_pos = get_pos(word[length], 0);

		while(current_pos > 0 && length <= order){
			if(length+1 < order){
				int next_pos = get_pos(word[length+1], current_pos);
				if(next_pos > 0){
					current_pos = next_pos;
					length++;
				}else{
					break;
				}
			}else{
				break;
			}
		}
		return current_pos;
	}else{
		return 0;
	}
}

bool DA::checkinput(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence){
	bool probres = prob_check(n-1,n,ngram,prob);
	bool bowres = true;
	if(bow_presence){
		bowres = da[ngram[n-1]%datotal]->bow_check(n,ngram,bow);
	}

	if(probres && bowres) return true;
	else return false; 
}

void DA::value_set(std::set<float> *value_set){
	float tmp_val=0.0;
	unsigned pos=0,count=1;

	varray_size = value_set->size()+1;
	vtable_size = (unsigned)(varray_size*1.4);

	value_array = new float[varray_size];
	value_table = new unsigned[vtable_size];

	value_array[0] = -99;
	for(unsigned i=0;i<vtable_size;i++){
		value_table[i]=INT_MAX;
	}

	std::set<float>::iterator it = value_set->begin();
	while(it!=value_set->end()){
		tmp_val = (*it);
		value_array[count] = tmp_val;
		pos = value_insert(tmp_val);
		value_table[pos]=count;

		++it;
		count++;
	}
}

void DA::value_replace()
{
	for(unsigned i=0;i<array_size;i++){
		if(check_array[i]<0){
			check_array[i]=INT_MIN;
		}

		if(value_id[i]!=-1){
			check_array[i]=0-value_id[i];
		}
	}
}

void DA::det_base(int *word,float *val,unsigned amount,unsigned now){ 
	unsigned *pos = new unsigned[amount];
	unsigned minindex = 0;
	unsigned maxindex = 0;

	for(size_t i = 1; i < amount; i++){
		if(word[i] < word[minindex]){
			minindex = i;
		}
		if(word[i] > word[maxindex]){
			maxindex = i;
		}
	}

	int base=first_empty_index-word[minindex];
	while(base < 0){
		base-=check_array[base+word[minindex]];
	}

	unsigned k=0; 
	while(k < amount){
		pos[k]=base+word[k];

		if(pos[k] < array_size && check_array[pos[k]]>=0){
			base-=check_array[base+word[minindex]];
			k=0;
		}else{
			k++;
		}
	}

	base_array[now].base_val=base;
	if(now==0){
		check_array[0]=0;
	}

	unsigned max_jump = base + word[maxindex]+1;   
	if(max_jump > array_size){
		resize_array(array_size*2 > max_jump ? array_size*2 : max_jump*2 );
	}

	for(unsigned i=0;i < amount; i++){
		unsigned next_index = pos[i] - check_array[pos[i]];

		unsigned int pre_index;
		if(pos[i]==first_empty_index){
			base_array[next_index].base_val = INT_MIN;
			first_empty_index = first_empty_index-check_array[first_empty_index];
		}else{
			pre_index = pos[i]+base_array[pos[i]].base_val;

			base_array[next_index].base_val += base_array[pos[i]].base_val;
			check_array[pre_index] += check_array[pos[i]];
		}

		check_array[pos[i]]=now;
		if(val!=NULL) base_array[pos[i]].logprob = val[i];
	}

	if(max_index < pos[maxindex]){ 
		max_index=pos[maxindex];
	}

	delete [] pos;
}

int DA::get_pos(int word,unsigned now){
	if(word<0){
		return -1;
	}

	unsigned dest=word+base_array[now].base_val;
	if(dest>max_index){
		return -1;
	}else if(check_array[dest]==(int)now){
		return dest;
	}else{
		return -1;
	}
}

int DA::get_terminal(unsigned now)
{
	return 1+base_array[now].base_val;
}

unsigned DA::value_insert(float tmp_val)
{
	unsigned tmp_pos=value_hash(tmp_val);

	while(1){
		if(value_table[tmp_pos]==INT_MAX){
			break;
		}else {
			tmp_pos++;
			if(tmp_pos==vtable_size){
				tmp_pos=0;
			}
		}
	}
	return tmp_pos;
}

unsigned DA::value_find(float tmp_val)
{
	unsigned tmp_pos=value_hash(tmp_val);
	while(1){
		if(value_table[tmp_pos]==INT_MAX){
			break;
		}else if(value_array[value_table[tmp_pos]]==tmp_val){
			return value_table[tmp_pos];
		}else {
			tmp_pos++;
			if(tmp_pos==vtable_size){
				tmp_pos=0;
			}
		}
	}
	return INT_MAX;
}

void DA::resize_array(unsigned newarray_size){
	_base *temp_base = new _base[newarray_size];
	memcpy(temp_base,base_array,array_size*sizeof(_base));
	delete [] base_array;
	base_array = temp_base;

	int *temp = new int[newarray_size];
	memcpy(temp,check_array,array_size*sizeof(int));
	delete [] check_array;
	check_array = temp;

	temp = new int[newarray_size];
	memcpy(temp,value_id,array_size*sizeof(int));
	delete [] value_id;
	value_id = temp;

	memset(base_array+array_size,0xFF,(newarray_size-array_size)*sizeof(_base));
	memset(check_array+array_size,0xFF,(newarray_size-array_size)*sizeof(int));
	memset(value_id+array_size,0xFF,(newarray_size-array_size)*sizeof(int));

	array_size = newarray_size;
}

bool DA::prob_check(int length, int order, unsigned int *ngram, float prob){
	int pos=0;

	while(length>0){
		pos = get_pos(ngram[-1+length--],pos);
		if(pos < 0){
			logger << "DA[" << daid << "][prob] bow_pos error!" << Logger::ende;
			return false;
		}
	}
	pos = get_terminal(pos);
	if(pos < 0){
		logger << "DA[" << daid << "][prob] terminal_pos error!" << Logger::ende;
		return false;
	}
	pos = get_pos(ngram[order-1],pos);
	if(pos < 0){
		logger << "DA[" << daid << "][prob] prob_pos error!" << Logger::ende;
		return false;
	}

	if(base_array[pos].logprob == prob){
		return true;
	}else{
		logger << "DA[" << daid << "][prob] prob value error! answer=" << prob << " stored=" << base_array[pos].logprob << Logger::ende;
		return false;
	}

	return false;
}

bool DA::bow_check(int length,unsigned int *ngram,float bow){
	int pos=0;
	float tmp_bow=0.0;

	while(length > 0){
		pos = get_pos(ngram[-1+length--],pos);
		if(pos < 0){
			logger << "DA[" << daid << "][bow] bow_pos error!" << Logger::ende;
			return false;
		}
	}

	pos = get_terminal(pos);
	if(pos < 0){
		logger << "DA[" << daid << "][bow] bow_terminal_pos error!" << Logger::ende;
		return false;
	}

	tmp_bow = value_array[-check_array[pos]];
	if(tmp_bow == bow){
		return true;
	}else{
		logger << "DA[" << daid << "][bow] bow value error! answer=" << bow << " stored=" << tmp_bow << Logger::ende;
		return false;
	}

	return false;
}

unsigned DA::value_hash(float tmp_val)
{
	unsigned int value = 0;
	LMMurmur::MurmurHash3_x86_32(&tmp_val,sizeof(float),0x9e3779b9,&value); 
	return value%vtable_size;
}

