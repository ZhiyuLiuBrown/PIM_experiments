/*
 * MyFCSkipList.h
 *
 *  Created on: Aug 8, 2016
 *      Author: zhiyul
 */

#ifndef TEST_MYFCSKIPLIST_H_
#define TEST_MYFCSKIPLIST_H_

#include "../framework/cpp_framework.h"

using namespace CCP;

class MyFCSkipList : public ITest {
protected://consts
	static final int _MAX_LEVEL	= 25;
	int num_combiners;
	int num_combined_reqs;

protected://types

	class Node {
	public:
		int		_key;
		int		_top_level;
		Node* 	_next[_MAX_LEVEL];

	public:

		Node(final int inKey) {
			_key = inKey;
			_top_level = _MAX_LEVEL;

			for (int i = 0; i < _top_level; i++)
				_next[i] = null;
		}

		Node(final int inKey, final int height) {
			_key = inKey;
			_top_level = height;

			for (int i = 0; i < _top_level; i++)
				_next[i] = null;
		}

		~Node() {
		}
	};

protected://SkipList fields
	VolatileType<_u64>	_random_seed;
	Node*				_head;
	Node* 				_tail;
	int 				_max_level; //
	int 				_est_size;


protected://Flat Combining fields

	AtomicInteger	_fc_lock;
	char							_pad1[CACHE_LINE_SIZE];
	final int		_NUM_REP;
	final int		_REP_THRESHOLD;
	Random 			_level_rand;

	Node*			preds[_MAX_LEVEL + 1];
	Node*			succs[_MAX_LEVEL + 1];

	//SlotInfo*		_saved_remove_node[1024];

protected://methods
	inline_ int randomLevel() {

#if 0
		int x = (int)(_random_seed.get()  & 0xFFFFFFUL);
		std::cerr << x << " ";

		x ^= x << 13;
		x ^= x >> 17;

		std::cerr << _random_seed.get() << " ";
		_random_seed.set( x ^= x << 5 );
#endif


		int x = _level_rand.nextInt(1 << _max_level);
		if ((x & 0x80000001) != 0) {// test highest and lowest bits
			//printf("0\n");  fflush(stdout);
			return 1;
		}
		int level = 2;
		while (((x >>= 1) & 1) != 0)
			++level;
		//printf("%d\n",level);  fflush(stdout);
		if(level > (_max_level-1))
			return (_max_level-1);
		else
			return level;
	}

	inline_ Node* find(final int key) {
		Node* pPred;
		Node* pCurr;
		pPred = _head;
		Node* found_node = null;

		for (int iLevel = _max_level - 1; iLevel >= 0; --iLevel) {

			pCurr = pPred->_next[iLevel];

			while (key > pCurr->_key) {
				pPred = pCurr;
				pCurr = pPred->_next[iLevel];
			}

			if (null == found_node && key == pCurr->_key) {
				found_node = pCurr;
			}

			preds[iLevel] = pPred;
			succs[iLevel] = pCurr;
		}

		return found_node;
	}


	inline_ void flat_combining() {

		//int num_removed = 0;
		num_combiners++;

		for (int iTry=0;iTry<_NUM_REP; ++iTry) {

			int num_changes=0;
			SlotInfo* curr_slot = _tail_slot.get();
			Memory::read_barrier();

			int count = 0;

			while(null != curr_slot) {
				if(curr_slot->_is_request_valid) {
					num_combined_reqs++;
					final int inValue = curr_slot->_req_ans;
					final int op = curr_slot->_op;
					++num_changes;
					Node* node_found = find(inValue);

					if (op == _OP_ADD) {
						if (null != node_found) {
							///////////++(node_found->_counter);
							curr_slot->_req_ans = _NULL_VALUE; //already existed
						}

						else {
							final int top_level = randomLevel();

							// first link succs ........................................
							// then link next fields of preds ..........................
							Node* new_node = new Node(inValue, top_level);
							Node** new_node_next = new_node->_next;
							Node** curr_succ = succs;
							Node** curr_preds = preds;

							for (int level = 0; level < top_level; ++level) {
								*new_node_next = *curr_succ;
								(*curr_preds)->_next[level] = new_node;
								++new_node_next;
								++curr_succ;
								++curr_preds;
							}

							curr_slot->_req_ans = 1; //add succeeds
						}
					}

					else if (op == _OP_DEL) {

						if (null == node_found) {
							curr_slot->_req_ans = _NULL_VALUE; //not found
						}

						else {
							// preds and succs are set in the find method
							for (int level = 0; level < node_found->_top_level;	++level) {
								preds[level]->_next[level] = node_found->_next[level];
							}

						delete node_found;
						curr_slot->_req_ans = 1; //delete succeeded
						}

						//++num_removed;
					}

					else { //op == _OP_CONTAIN
						if (node_found == null)
							curr_slot->_req_ans = _NULL_VALUE; //not found
						else
							curr_slot->_req_ans = 1; //found
					}

					curr_slot->_is_request_valid = false;//
				}

				curr_slot = curr_slot->_next;
				count++;
			} //while on slots

			if (count > _gNumThreads + 1)
				std::cerr << count << " slots\n";

			if(num_changes < _REP_THRESHOLD)
				break;
		}

	}

public://methods

	MyFCSkipList(int minVal, int maxVal) :
#if 1
		_NUM_REP(1),
#else
	   _NUM_REP( Math::Min(2, _NUM_THREADS)),
#endif
	  _REP_THRESHOLD((int)(Math::ceil(_NUM_THREADS/(1.7))))
	{
		int size = maxVal - minVal;
		_max_level = _MAX_LEVEL;

		for (int i = 1; i < _MAX_LEVEL; i++) {//_max_level depends on size
			if (size < (1 << i) ) {
				_max_level = i;
				break;
			}
		}

		_head = new Node(INT_MIN, _max_level);
		_tail = new Node(INT_MAX, _max_level);

		//initialize head to point to tail .....................................
		for (int iLevel = 0; iLevel < _head->_top_level; ++iLevel)
			_head->_next[iLevel] = _tail;

		//initialize the slots .........................................
		_tail_slot.set(null);

		ITest::SlotInfo* volatile local_slot = new ITest::SlotInfo();
		int currVal;
		Random	rand;


		//////////////////////////////////////////////////
		//for (int i = 0; i < size * 13 / 10; i++) {
		//	currVal = rand.nextInt(size) + minVal;
		//	fcAdd(0, currVal, local_slot);
		//}

		int node_count = 0;
		while (node_count < size / 2) {
			currVal = rand.nextInt(size) + minVal;
			if (fcAdd(0, currVal, local_slot))
				node_count++;
		}

		_est_size = size / 2;

		num_combiners = 1;
		num_combined_reqs = 1;

		///////////////////////////////////////////////////////////////////
		//Node* currNode = _head;
		//while (currNode != null) {
		//	std::cerr << currNode->_key << " with height " << currNode->_top_level << "\n";
		//	currNode = currNode->_next[0];
		//}
		///////////////////////////////////////////////////////////////////
	}

	~MyFCSkipList() {
		Node* currNode = _head;
		Node* nextNode;
		int count = 0;

		while (currNode != _tail) {
			nextNode = currNode->_next[0];
			delete currNode;
			currNode = nextNode;
			count++;
		}

		delete _tail;

		if (count * 8 / 10 > _est_size || count / 8 * 10 < _est_size)
			std::cerr << count + 1 << " nodes deleted in cleanup\n";

		std::cerr << " FC rate: " << ((double) num_combined_reqs) / num_combiners;
	}

public://methods

	inline_ int singleFCOP(final int iThread, final int inValue, final int op, SlotInfo* local_slot) {
		SlotInfo* my_slot = local_slot;

		my_slot->_op = op;
		my_slot->_req_ans = inValue;
		my_slot->_is_request_valid = true;

		if (!my_slot->_is_slot_linked && my_slot->_is_request_valid) {
			enq_slot(my_slot);
		}

		do {
			boolean is_cas = false;
			if (lock_fc(_fc_lock, is_cas)) {

				if (!my_slot->_is_slot_linked && my_slot->_is_request_valid)
					enq_slot(my_slot);

				machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				machine_end_fc(iThread);
				return my_slot->_req_ans;
			} else {
				Memory::write_barrier();

				while (my_slot->_is_request_valid && 0 != _fc_lock.getNotSafe()) { //
					thread_wait(iThread);

					if (!my_slot->_is_slot_linked && my_slot->_is_request_valid)
						enq_slot(my_slot);
				}
				Memory::read_barrier();

				if (!my_slot->_is_request_valid) {
					return my_slot->_req_ans;
				}
			}
		} while (true);
	}

	//OPs using single FC......................................................................
	int fcContain(final int iThread, final int inValue,
			SlotInfo* local_slot) {
		return singleFCOP(iThread, inValue, _OP_CONTAIN, local_slot);
	}

	int fcAdd(final int iThread, final int inValue,
			SlotInfo* local_slot) {
		return singleFCOP(iThread, inValue, _OP_ADD, local_slot);
	}

	int fcDelete(final int iThread, final int inValue,
			SlotInfo* local_slot) {
		return singleFCOP(iThread, inValue, _OP_DEL, local_slot);
	}


public://methods

	int contain(final int iThread, final int inValue){return 0;};
	boolean add(final int iThread, final int inValue){return true;};
	int remove(final int iThread, final int inValue){return 0;};

public://methods

	int size() {
		return 0;
	}

	final char* name() {
		return "FCSkipList";
	}

};




#endif /* TEST_MYFCSKIPLIST_H_ */
