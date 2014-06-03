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

EmbeddedDA::EmbeddedDA(
		unsigned char daid, 
		unsigned char datotal, 
		ValueArray &value_array, 
		EmbeddedDA **neighbours, 
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
		da_array[i].check.check_val = -1;
		value_id[i] = -1;
	}  
}

EmbeddedDA::EmbeddedDA(FILE *fp, ValueArray &value_array, EmbeddedDA **neighbours, unsigned char order, Logger &logger)
	: value_array(value_array), logger(logger){
	DA::context_size = order-1;
	fread(&daid,sizeof(unsigned char),1,fp);
	fread(&datotal,sizeof(unsigned char),1,fp);
	da = neighbours;
	fread(&max_index,sizeof(unsigned),1,fp);
	array_size = max_index+1;

	da_array = new DAPair[array_size];

	fread(da_array, sizeof(DAPair), array_size, fp);

	value_id = NULL;
	first_empty_index=0;
	logger << "EmbeddedDA[" << daid << "] da-size: " << array_size << Logger::endi;
}

EmbeddedDA::~EmbeddedDA(){
	if(da_array != NULL) delete [] da_array;
	if(value_id != NULL) delete [] value_id;
}

void EmbeddedDA::make_da(std::string &pathtotreefile, ValueArrayIndex *value_array_index, Vocabulary &vocab)
{
	TreeFile tf(pathtotreefile, vocab);
	size_t unigram_type = vocab.size();
	resize_array(unigram_type*20);
	da_array[0].base.base_val = 0;

	size_t total = tf.get_totalsize();
	logger << "EmbeddedDA[" << daid << "] total=" << total << Logger::endi;
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
			logger << "EmbeddedDA[" << daid << "] " << (i+1)/tenpercent << "0% done." << Logger::endi;
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
			if(historysize!=0 && history[historysize-1]==1){ // context ends for <#>.
				det_base(words, values, wordssize, now);
			}else{
				det_base(words, NULL, wordssize, now);
				if(terminal_pos!=(size_t)-1){
					unsigned terminal=get_terminal(now);
					value_id[terminal] = value_array_index->lookup(values[terminal_pos]);
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
		det_base(words, values, wordssize, now);
	}else{
		det_base(words, NULL, wordssize, now);
		if(terminal_pos!=(size_t)-1){
			unsigned terminal=get_terminal(now);
			value_id[terminal] = value_array_index->lookup(values[terminal_pos]);
		}
	}

	replace_value();
	delete [] history;
	delete [] words;
	delete [] values;
}

void EmbeddedDA::dump(FILE *fp)
{
	fwrite(&daid,sizeof(unsigned char),1,fp);
	fwrite(&datotal,sizeof(unsigned char),1,fp);
	fwrite(&max_index,sizeof(unsigned),1,fp);
	fwrite(da_array, sizeof(DAPair), max_index+1, fp);

	logger << "EmbeddedDA[" << daid << "] da-size: " << max_index << Logger::endi;
}

float EmbeddedDA::get_prob(VocabId *word, unsigned char order){
	float prob = 0.0;
	float bow = 0.0;
	int current_pos=0;
	int terminal;
	unsigned int prob_pos, next;
	VocabId target_word = word[0];
	for(unsigned int i = 1; i < order; i++){
		next = da_array[current_pos].base.base_val+word[i];
		if(next < array_size && da_array[next].check.check_val == current_pos){
			current_pos = next;

			terminal = get_terminal(next);
			prob_pos = da_array[terminal].base.base_val+target_word;
			if(prob_pos < array_size && da_array[prob_pos].check.check_val == terminal){
				prob = da_array[prob_pos].base.logprob;
				if(prob > 0) prob = -prob;
				bow = 0.0;
			}else{
				bow += value_array[-da_array[terminal].check.check_val];
			}
		}else{
			break;
		}
	}

	if(prob==0.0){
		// case for backoff to unigrams.
		EmbeddedDA *unigram_da = da[target_word%datotal];
		terminal = unigram_da->get_terminal(0);
		prob_pos = unigram_da->da_array[terminal].base.base_val+target_word;
		if(prob_pos < unigram_da->array_size 
				&& unigram_da->da_array[prob_pos].check.check_val == terminal){
			prob = unigram_da->da_array[prob_pos].base.logprob;
			return ((prob>0)?-prob:prob)+bow;
		}else{
			return DALM_OOV_PROB+bow;
		}
	}else{
		return prob+bow;
	}
}

float EmbeddedDA::get_prob(VocabId word, State &state){
	float bow = 0.0;
	float prob = DALM_OOV_PROB;
	unsigned short count = state.get_count();
	unsigned int prob_pos, next;
	int pos = 0;
	int terminal;

	state.set_count(0);

	// case for backoff to unigrams.
	EmbeddedDA *unigram_da = da[word%datotal];
	terminal = unigram_da->get_terminal(0);
	prob_pos = unigram_da->da_array[terminal].base.base_val+word;
	if(prob_pos < unigram_da->array_size 
			&& unigram_da->da_array[prob_pos].check.check_val == terminal){
		prob=unigram_da->da_array[prob_pos].base.logprob;
		if(prob<0){
		 	state.set_count(1);
		}else{
			prob=-prob;
		}
	}

	for(unsigned int i = 0; i < count; i++){
		next = da_array[pos].base.base_val+state.get_word(i);
		state.set_bow(i, 0.0);
		if(next < array_size && da_array[next].check.check_val == pos){
			pos = next;

			terminal = get_terminal(next);
			prob_pos = da_array[terminal].base.base_val+word;
			if(prob_pos < array_size && da_array[prob_pos].check.check_val == terminal){
				prob = da_array[prob_pos].base.logprob;
				bow = 0.0;
				if(prob < 0){
					state.set_count(std::min<unsigned char>(i+2,context_size));
				}else{
					prob=-prob;
				}
			}else{
				bow += value_array[-da_array[terminal].check.check_val];
			}
		}else{
			break;
		}
	}
	state.push_word(word);

	return prob+bow;
}

float EmbeddedDA::get_prob(VocabId word, State &state, Fragment &fnew){
	float bow = 0.0;
	float prob = DALM_OOV_PROB;
	unsigned short count = state.get_count();
	unsigned int prob_pos, next;
	int pos = 0;
	int terminal;
	bool finalized = false;

	fnew.status.word=word;
	state.set_count(0);

	// case for backoff to unigrams.
	fnew.status.pos=0;
	EmbeddedDA *unigram_da = da[word%datotal];
	terminal = unigram_da->get_terminal(0);
	prob_pos = unigram_da->da_array[terminal].base.base_val+word;
	if(prob_pos < unigram_da->array_size 
			&& unigram_da->da_array[prob_pos].check.check_val == terminal){
		prob=unigram_da->da_array[prob_pos].base.logprob;
		if(prob<0){
		 	state.set_count(1);
		}else{
			prob = -prob;
		}
	}
	for(unsigned int i = 0; i < count; i++){
		next = da_array[pos].base.base_val+state.get_word(i);
		state.set_bow(i, 0.0);
		if(next < array_size && da_array[next].check.check_val == pos){
			pos = next;
			fnew.status.pos=pos;

			terminal = get_terminal(next);
			prob_pos = da_array[terminal].base.base_val+word;
			if(prob_pos < array_size && da_array[prob_pos].check.check_val == terminal){
				prob = da_array[prob_pos].base.logprob;
				bow = 0.0;
				if(prob < 0){
					state.set_count(std::min<unsigned char>(i+2,context_size));
				}else{
					prob = -prob;
				}
			}else{
				bow += value_array[-da_array[terminal].check.check_val];
			}
		}else{
			finalized = true;
			break;
		}
	}
	state.push_word(word);

	prob+=bow;
	return (finalized)?-prob:prob;
}

float EmbeddedDA::get_prob(const Fragment &fprev, State &state, Gap &gap){
	size_t g = gap.get_gap();
	unsigned char depth = g;
	unsigned char count = gap.get_count();
	bool &finalized = gap.is_finalized();
	bool &extended = gap.is_extended();
	gap.set_count(depth);

	if(count <= g){
		finalized=true;
		extended=false;
		return 0.0;
	}
	extended=false;
	finalized=false;

	int pos = fprev.status.pos;
	float prob_prev;
	if(g==0){
		unsigned char prevdaid = fprev.status.word%datotal;
		int terminal = da[prevdaid]->get_terminal(pos);
		int probindex = da[prevdaid]->da_array[terminal].base.base_val+fprev.status.word;

		if(da[prevdaid]->da_array[probindex].check.check_val == terminal){
			prob_prev = da[prevdaid]->da_array[probindex].base.logprob;
		}else{
			prob_prev = 0.0;
		}
	}else{
		int terminal = get_terminal(pos);
		int probindex = da_array[terminal].base.base_val+fprev.status.word;

		if(da_array[probindex].check.check_val == terminal){
			prob_prev = da_array[probindex].base.logprob;
		}else{
			prob_prev = 0.0;
		}
	}
	if(prob_prev>0)prob_prev=-prob_prev;
	float prob = prob_prev;
	float bow = 0.0;
	unsigned int prob_pos, next;
	int terminal;
 
	for(; depth < count; depth++){
		next = da_array[pos].base.base_val+state.get_word(depth);
		state.set_bow(depth, 0.0);
		if(next < array_size && da_array[next].check.check_val == pos){
			extended=true;
			pos = next;

			terminal = get_terminal(next);
			prob_pos = da_array[terminal].base.base_val+fprev.status.word;
			if(prob_pos < array_size && da_array[prob_pos].check.check_val == terminal){
				prob = da_array[prob_pos].base.logprob;
				bow = 0.0;
				if(prob < 0){
					gap.set_count(std::min<unsigned char>(depth+2,context_size));
				}else{
					prob=-prob;
				}
			}else{
				bow += value_array[-da_array[terminal].check.check_val];
			}
		}else{
			finalized=true;
			break;
		}
	}
	state.push_word(fprev.status.word);

	return prob+bow-prob_prev;
}

float EmbeddedDA::get_prob(const Fragment &fprev, State &state, Gap &gap, Fragment &fnew){
	size_t g = gap.get_gap();
	unsigned char depth = g;
	unsigned char count = gap.get_count();
	bool &finalized = gap.is_finalized();
	bool &extended = gap.is_extended();
	gap.set_count(depth);
	fnew = fprev;

	if(count <= g){
		finalized=true;
		extended=false;
		return 0.0;
	}
	extended=false;
	finalized=false;

	int pos = fprev.status.pos;
	float prob_prev;
	if(g==0){
		unsigned char prevdaid = fprev.status.word%datotal;
		int terminal = da[prevdaid]->get_terminal(pos);
		int probindex = da[prevdaid]->da_array[terminal].base.base_val+fprev.status.word;

		if(da[prevdaid]->da_array[probindex].check.check_val == terminal){
			prob_prev = da[prevdaid]->da_array[probindex].base.logprob;
		}else{
			prob_prev = 0.0;
		}
	}else{
		int terminal = get_terminal(pos);
		int probindex = da_array[terminal].base.base_val+fprev.status.word;

		if(da_array[probindex].check.check_val == terminal){
			prob_prev = da_array[probindex].base.logprob;
		}else{
			prob_prev = 0.0;
		}
	}

	if(prob_prev>0)prob_prev=-prob_prev;
	float prob = prob_prev;
	float bow = 0.0;
	unsigned int prob_pos, next;
	int terminal;

	for(; depth < count; depth++){
		next = da_array[pos].base.base_val+state.get_word(depth);
		state.set_bow(depth, 0.0);
		if(next < array_size && da_array[next].check.check_val == pos){
			extended=true;
			pos = next;
			fnew.status.pos = pos;

			terminal = get_terminal(next);
			prob_pos = da_array[terminal].base.base_val+fprev.status.word;
			if(prob_pos < array_size && da_array[prob_pos].check.check_val == terminal){
				prob = da_array[prob_pos].base.logprob;
				bow = 0.0;
				if(prob < 0){
					gap.set_count(std::min<unsigned char>(depth+2,context_size));
				}else{
					prob = -prob;
				}
			}else{
				bow += value_array[-da_array[terminal].check.check_val];
			}
		}else{
			finalized=true;
			break;
		}
	}
	state.push_word(fprev.status.word);

	return prob+bow-prob_prev;
}

void EmbeddedDA::init_state(VocabId *word, unsigned char order, State &state){
  for(unsigned char i = 0; i < order; i++){
		state.set_word(i, word[i]);
		state.set_bow(i,0.0);
	}

  state.set_count(order);
}

void EmbeddedDA::init_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &gap){
	/*
	unsigned char scount = state.get_count();
	unsigned char depth = gap.get_gap();
	unsigned char max_depth = std::min(
			(int)scount+depth,
			(int)context_size
	);
	unsigned char move_end = max_depth-depth;
	state.shift_words(0, move_end, depth);

	for(unsigned char i = state_prev.get_count(); i < gap.get_gap(); i++){
		state.set_word(i, fragments[gap.get_gap()-i-1].status.word);
	}
	*/

	state.set_count(gap.get_count());
}

/* depricated */
unsigned long int EmbeddedDA::get_state(VocabId *word, unsigned char order){
	size_t length=0;
	int current_pos = get_pos(word[length], 0);
	unsigned long int current_state = 0;

	while(current_pos > 0 && length <= order){
		if(length+1 < order){
			int next_pos = get_pos(word[length+1], current_pos);
			if(next_pos > 0){
				int terminal = get_terminal(current_pos);
				if(value_array[-da_array[terminal].check.check_val]!=0){
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

bool EmbeddedDA::checkinput(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence){
	bool probres = prob_check(n-1,n,ngram,prob);
	bool bowres = true;
	if(bow_presence){
		bowres = da[ngram[n-1]%datotal]->bow_check(n,ngram,bow);
	}

	if(probres && bowres) return true;
	else return false; 
}

void EmbeddedDA::replace_value()
{
	for(unsigned i=0;i<array_size;i++){
		if(da_array[i].check.check_val<0){
			da_array[i].check.check_val=INT_MIN;
		}

		if(value_id[i]!=-1){
			da_array[i].check.check_val=-value_id[i];
		}
	}
}

void EmbeddedDA::det_base(int *word,float *val,unsigned amount,unsigned now){ 
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
		base-=da_array[base+word[minindex]].check.check_val;
	}

	unsigned k=0; 
	while(k < amount){
		pos[k]=base+word[k];

		if(pos[k] < array_size && da_array[pos[k]].check.check_val>=0){
			base-=da_array[base+word[minindex]].check.check_val;
			k=0;
		}else{
			k++;
		}
	}

	da_array[now].base.base_val=base;

	unsigned max_jump = base + word[maxindex]+1;   
	if(max_jump > array_size){
		resize_array(array_size*2 > max_jump ? array_size*2 : max_jump*2 );
	}

	for(unsigned i=0;i < amount; i++){
		unsigned next_index = pos[i] - da_array[pos[i]].check.check_val;

		unsigned int pre_index;
		if(pos[i]==first_empty_index){
			da_array[next_index].base.base_val = INT_MIN;
			first_empty_index = first_empty_index-da_array[first_empty_index].check.check_val;
		}else{
			pre_index = pos[i]+da_array[pos[i]].base.base_val;

			da_array[next_index].base.base_val += da_array[pos[i]].base.base_val;
			da_array[pre_index].check.check_val += da_array[pos[i]].check.check_val;
		}

		da_array[pos[i]].check.check_val=now;
		if(val!=NULL) da_array[pos[i]].base.logprob = val[i];
	}

	if(max_index < pos[maxindex]){ 
		max_index=pos[maxindex];
	}

	delete [] pos;
}

int EmbeddedDA::get_pos(int word,unsigned now){
	if(word<0){
		return -1;
	}

	unsigned dest=word+da_array[now].base.base_val;
	if(dest>max_index){
		return -1;
	}else if(da_array[dest].check.check_val==(int)now){
		return dest;
	}else{
		return -1;
	}
}

int EmbeddedDA::get_terminal(unsigned now)
{
	return 1+da_array[now].base.base_val;
}

void EmbeddedDA::resize_array(unsigned newarray_size){
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

bool EmbeddedDA::prob_check(int length, int order, unsigned int *ngram, float prob){
	int pos=0;

	while(length>0){
		pos = get_pos(ngram[-1+length--],pos);
		if(pos < 0){
			logger << "EmbeddedDA[" << daid << "][prob] bow_pos error!" << Logger::ende;
			return false;
		}
	}
	pos = get_terminal(pos);
	if(pos < 0){
		logger << "EmbeddedDA[" << daid << "][prob] terminal_pos error!" << Logger::ende;
		return false;
	}
	pos = get_pos(ngram[order-1],pos);
	if(pos < 0){
		logger << "EmbeddedDA[" << daid << "][prob] prob_pos error!" << Logger::ende;
		return false;
	}

	if(da_array[pos].base.logprob == prob || da_array[pos].base.logprob == -prob){
		return true;
	}else{
		logger << "EmbeddedDA[" << daid << "][prob] prob value error! answer=" << prob << " stored=" << da_array[pos].base.logprob << Logger::ende;
		return false;
	}

	return false;
}

bool EmbeddedDA::bow_check(int length,unsigned int *ngram,float bow){
	int pos=0;
	float tmp_bow=0.0;

	while(length > 0){
		pos = get_pos(ngram[-1+length--],pos);
		if(pos < 0){
			logger << "EmbeddedDA[" << daid << "][bow] bow_pos error!" << Logger::ende;
			return false;
		}
	}

	pos = get_terminal(pos);
	if(pos < 0){
		logger << "EmbeddedDA[" << daid << "][bow] bow_terminal_pos error!" << Logger::ende;
		return false;
	}

	tmp_bow = value_array[-da_array[pos].check.check_val];
	if(tmp_bow == bow){
		return true;
	}else{
		logger << "EmbeddedDA[" << daid << "][bow] bow value error! answer=" << bow << " stored=" << tmp_bow << Logger::ende;
		return false;
	}

	return false;
}

