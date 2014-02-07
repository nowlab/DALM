#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits.h>

#include <utility>

#include "treefile.h"
#include "da.h"
#include "value_array.h"
#include "value_array_index.h"

using namespace DALM;

DA::DA(
		size_t daid, 
		size_t datotal, 
		ValueArray &value_array, 
		DA **neighbours, 
		Logger &logger)
		: daid(daid), 
			datotal(datotal), 
			value_array(value_array), 
			da(neighbours), 
			logger(logger){
	array_size=3;
	da_array = new DAPair[array_size];
	value_id = new int[array_size];

	max_index=0;
	first_empty_index=1;

	for(unsigned i=0;i<array_size;i++){
		da_array[i].base.base_val = -1;
		da_array[i].check = -1;
		value_id[i] = -1;
	}  
}

DA::DA(FILE *fp, ValueArray &value_array, DA **neighbours, Logger &logger)
	: value_array(value_array), logger(logger){
	fread(&daid,sizeof(unsigned),1,fp);
	fread(&datotal,sizeof(unsigned),1,fp);
	da = neighbours;
	fread(&max_index,sizeof(unsigned),1,fp);
	array_size = max_index+1;

	da_array = new DAPair[array_size];

	fread(da_array, sizeof(DAPair), array_size, fp);

	value_id = NULL;
	first_empty_index=0;
	logger << "DA[" << daid << "] da-size: " << array_size << Logger::endi;
}

DA::~DA(){
	if(da_array != NULL) delete [] da_array;
	if(value_id != NULL) delete [] value_id;
}

void DA::make_da(TreeFile &tf, ValueArrayIndex &value_array_index, unsigned unigram_type)
{
	resize_array(unigram_type*20);
	da_array[0].base.base_val = 0;

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
				if(value_id[now]==0.0){
					value_id[now] = value_array_index.lookup(-0.0);
				}
				det_base(words, values, wordssize, now);
			}else{
				det_base(words, NULL, wordssize, now);
				if(terminal_pos!=(size_t)-1){
					unsigned terminal=get_terminal(now);
					value_id[terminal] = value_array_index.lookup(values[terminal_pos]);
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
		now = get_pos(history[j], now);
	}
	if(historysize!=0 && history[historysize-1]==1){
		if(value_id[now]==0.0){
			value_id[now] = value_array_index.lookup(-0.0);
		}
		det_base(words, values, wordssize, now);
	}else{
		det_base(words, NULL, wordssize, now);
		if(terminal_pos!=(size_t)-1){
			unsigned terminal=get_terminal(now);
			value_id[terminal] = value_array_index.lookup(values[terminal_pos]);
		}
	}

	replace_value();
	delete [] history;
	delete [] words;
	delete [] values;
}

void DA::dump(FILE *fp)
{
	fwrite(&daid,sizeof(unsigned),1,fp);
	fwrite(&datotal,sizeof(unsigned),1,fp);
	fwrite(&max_index,sizeof(unsigned),1,fp);
	fwrite(da_array, sizeof(DAPair), max_index+1, fp);

	logger << "DA[" << daid << "] da-size: " << max_index << Logger::endi;
}

float DA::get_prob(int *word,int order){
	int length=0;
	float tmp_bow = 0.0;
	float tmp_prob = 0.0;

	int unigram_daid = word[0]%datotal;
	int unigram_terminal = da[unigram_daid]->get_terminal(0);
	int unigram_prob_pos = da[unigram_daid]->get_pos(word[0], unigram_terminal);
	if(unigram_prob_pos > 0){
		tmp_prob = da[unigram_daid]->da_array[unigram_prob_pos].base.logprob;
	}else{ // DALM_OOV_PROB
		tmp_prob = DALM_OOV_PROB;
	}

	if(order > 1){
		int current_pos = get_pos(word[1], 0);
		length=1;

		while(current_pos > 0 && length < order){
			int terminal = get_terminal(current_pos);
			tmp_bow += value_array[-da_array[terminal].check];
			int prob_pos = get_pos(word[0], terminal);
			if(prob_pos > 0){
				tmp_prob = da_array[prob_pos].base.logprob;
				tmp_bow = 0.0;
			}

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
		return tmp_prob+tmp_bow;
	}else{
		return tmp_prob;
	}
}

float DA::get_prob(int word, State &state){
	_bowval bowval;

	float bow = 0.0;
	float prob = DALM_OOV_PROB;
	unsigned short scount = state.get_count();

	int i = scount-1;
	for(; i >= 0; i--){
		StateId &sid = state[i];
		int prob_pos = get_pos(word, sid);
		if(prob_pos > 0){
			prob = da_array[prob_pos].base.logprob;
			break;
		}else{
			bow += value_array[-da_array[sid].check];
		}
	}

	state.set_count(0);
	state.push_word(word);

	int nextid = word%datotal;
	state.set_daid(nextid);
	int pos = 0;
	int terminal = da[nextid]->get_terminal(pos);
	if(i == -1){
		int prob_pos = da[nextid]->get_pos(word, terminal);
		if(prob_pos > 0){
			prob = da[nextid]->da_array[prob_pos].base.logprob;
			i++;
		}
	}

	scount++;
	size_t max_depth = (scount>state.get_order())?state.get_order():scount;
	//std::cout << "max_depth=" << max_depth << std::endl;
	for(size_t i = 0; i < max_depth; i++){
		int next = da[nextid]->get_pos(state.get_word(i), pos);
		if(next > 0){
			terminal = da[nextid]->get_terminal(next);
			state[i] = (StateId) terminal;
			bowval.bow = value_array[-da[nextid]->da_array[terminal].check];
			if(bowval.bits!=0x80000000UL){
				state.set_count(i+1);
			}
			pos = next;
		}else{
			break;
		}
	}

	return prob+bow;
}

void DA::init_state(int *word, unsigned short order, State &state){
	_bowval bowval;
	int pos = 0;
	state.set_count(0);
	state.set_daid(daid);
	for(unsigned short i = 0; i < order; i++){
		int next = get_pos(word[i], pos);
		if(next > 0){
			state.set_word(i, word[i]);
			int terminal = get_terminal(next);
			state[i] = (StateId) terminal;
			bowval.bow = value_array[-da_array[terminal].check];
			if(bowval.bits!=0x80000000UL){
				state.set_count(i+1);
			}
			pos=next;
		}else{
			break;
		}
	}
}

/* depricated */
unsigned long int DA::get_state(int *word,int order){
	int length=0;
	int current_pos = get_pos(word[length], 0);
	unsigned long int current_state = 0;

	while(current_pos > 0 && length <= order){
		if(length+1 < order){
			int next_pos = get_pos(word[length+1], current_pos);
			if(next_pos > 0){
				int terminal = get_terminal(current_pos);
				if(value_array[-da_array[terminal].check]!=0){
					current_state = next_pos;
				}
				current_pos = next_pos;
				length++;
			}else{
				break;
			}
		}else{
			break;
		}
	}
	return current_state;
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

void DA::replace_value()
{
	for(unsigned i=0;i<array_size;i++){
		if(da_array[i].check<0){
			da_array[i].check=INT_MIN;
		}

		if(value_id[i]!=-1){
			da_array[i].check=-value_id[i];
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
		base-=da_array[base+word[minindex]].check;
	}

	unsigned k=0; 
	while(k < amount){
		pos[k]=base+word[k];

		if(pos[k] < array_size && da_array[pos[k]].check>=0){
			base-=da_array[base+word[minindex]].check;
			k=0;
		}else{
			k++;
		}
	}

	da_array[now].base.base_val=base;
	if(now==0){
		da_array[0].check=0;
	}

	unsigned max_jump = base + word[maxindex]+1;   
	if(max_jump > array_size){
		resize_array(array_size*2 > max_jump ? array_size*2 : max_jump*2 );
	}

	for(unsigned i=0;i < amount; i++){
		unsigned next_index = pos[i] - da_array[pos[i]].check;

		unsigned int pre_index;
		if(pos[i]==first_empty_index){
			da_array[next_index].base.base_val = INT_MIN;
			first_empty_index = first_empty_index-da_array[first_empty_index].check;
		}else{
			pre_index = pos[i]+da_array[pos[i]].base.base_val;

			da_array[next_index].base.base_val += da_array[pos[i]].base.base_val;
			da_array[pre_index].check += da_array[pos[i]].check;
		}

		da_array[pos[i]].check=now;
		if(val!=NULL) da_array[pos[i]].base.logprob = val[i];
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

	unsigned dest=word+da_array[now].base.base_val;
	if(dest>max_index){
		return -1;
	}else if(da_array[dest].check==(int)now){
		return dest;
	}else{
		return -1;
	}
}

int DA::get_terminal(unsigned now)
{
	return 1+da_array[now].base.base_val;
}

void DA::resize_array(unsigned newarray_size){
	DAPair *temp_array = new DAPair[newarray_size];
	memcpy(temp_array, da_array, array_size*sizeof(DAPair));
	delete [] da_array;
	da_array = temp_array;

	int *temp = new int[newarray_size];
	memcpy(temp, value_id, array_size*sizeof(int));
	delete [] value_id;
	value_id = temp;

	memset(da_array+array_size,0xFF,(newarray_size-array_size)*sizeof(DAPair));
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

	if(da_array[pos].base.logprob == prob){
		return true;
	}else{
		logger << "DA[" << daid << "][prob] prob value error! answer=" << prob << " stored=" << da_array[pos].base.logprob << Logger::ende;
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

	tmp_bow = value_array[-da_array[pos].check];
	if(tmp_bow == bow){
		return true;
	}else{
		logger << "DA[" << daid << "][bow] bow value error! answer=" << bow << " stored=" << tmp_bow << Logger::ende;
		return false;
	}

	return false;
}

