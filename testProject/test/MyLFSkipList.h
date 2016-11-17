#ifndef __LF_SKIP_LIST_SL__
#define __LF_SKIP_LIST_SL__

// Note (Irina): This is the real Lock-Free Skiplist from Maurice's book



//------------------------------------------------------------------------------
// File    : LFSkipList.h
// Author  : Ms.Moran Tzafrir; email: morantza@gmail.com; phone: +972-505-779961
// Written : 13 April 2009
//
// Lock Free Skip List
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------
// TODO:
//------------------------------------------------------------------------------
//*

#include "../framework/cpp_framework.h"

using namespace CCP;


class MyLFSkipList : public ITest {
protected://static
	int _num_threads;
	static final int _MAX_LEVEL = 25;
	int _max_level;
	//PaddedUInt not_found[_num_threads];
	//PaddedUInt not_added[_num_threads];

	//PaddedUInt failed_cas[_num_threads];

	int found_count;//////////////

protected://types

	class Node {
	public:
		final int								_key;
		int										_topLevel;
		AtomicMarkableReference<Node>			_next[_MAX_LEVEL];//now using fixed array/////AtomicMarkableReference<Node>* final	_next;

		Node(final int key, final int height)
		:	_key(key),
			_topLevel(height)//,
			//_next(new AtomicMarkableReference<Node>[height + 1])
		{}// constructor for regular nodes

		~Node() {
			//now using fixed array//////delete[] _next;
		}
	};

protected://fields
	//VolatileType<_u64>	_random_seed;
	VolatileType<Node*>	_head;
	VolatileType<Node*>	_tail;
	//TTASLock			_lock_removemin;
	Random*                tprng;
	int 				_est_size;

protected://methods
#if 0
	int randomLevel() {
		int x = (int)(_random_seed.get()  & 0xFFFFFFUL);
		x ^= x << 13;
		x ^= x >> 17;
		_random_seed.set( x ^= x << 5 );
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
#endif

	inline_ int randomLevel(int iThread) {
		int x = tprng[iThread].nextInt(1 << _max_level);
		//x ^= x << 13;
		//x ^= x >> 17;
		if ((x & 0x80000001) != 0) {// test highest and lowest bits
			//printf("0\n");  fflush(stdout);
			return 1;
		}
		int level = 2;
		while (((x >>= 1) & 1) != 0)
			++level;
		//printf("%d\n",level);  fflush(stdout);
		if (level > (_max_level - 1))
			return (_max_level - 1);
		else
			return level;
	}

	void findAndClean(int iThread, final int key) {
		boolean marked = false;
		boolean snip;
		Node* pPred;
		Node* pCurr;
		Node* pSucc;
		Node* pPredFirst;

	find_retry:
		int iLevel;
		while (true) {
			pPredFirst = _head;
			//pPred = _head;
			for (iLevel = _max_level - 1; iLevel >= 0; --iLevel) {
				pPred = pPredFirst;
				pCurr = pPred->_next[iLevel].getReference();
				while (true) {
					pSucc = pCurr->_next[iLevel].get(&marked);
					while (marked) {           // replace curr if marked
						snip = pPred->_next[iLevel].compareAndSet(pCurr, pSucc, false, false);
						if (!snip) {
							//////////////////////////////failed_cas[iThread].val++;
							goto find_retry;
						}

						pCurr = pPred->_next[iLevel].getReference();
						pSucc = pCurr->_next[iLevel].get(&marked);
					}
					if (pCurr->_key < key) { // move forward same iLevel
						pPred = pCurr;
						pPredFirst = pPred;
						pCurr = pSucc;
					}
					else if (pCurr->_key == key) {
						pPred = pCurr;
						pCurr = pSucc;
					}
					else {
						break; // move to _next iLevel
					}
				}

			}

			return;
		}
	}

	Node* find(int iThread, final int key, Node** preds, Node** succs) {
		boolean marked = false;
		boolean snip;
		Node* pPred;
		Node* pCurr;
		Node* pSucc;

	find_retry:
		int iLevel;
		while (true) {
			pPred = _head;
			for (iLevel = _max_level-1; iLevel >= 0; --iLevel) {
				pCurr = pPred->_next[iLevel].getReference();
				while (true) {
					pSucc = pCurr->_next[iLevel].get(&marked);
					while (marked) {           // replace curr if marked
						snip = pPred->_next[iLevel].compareAndSet(pCurr, pSucc, false, false);
						if (!snip) {
							///////////////////////////////failed_cas[iThread].val++;
							goto find_retry;
						}

						//if(++(pCurr->_level_removed) == pCurr->_topLevel)
						//	pCurr.retire();

						pCurr = pPred->_next[iLevel].getReference();
						pSucc = pCurr->_next[iLevel].get(&marked);
					}
					if (pCurr->_key < key) { // move forward same iLevel
						pPred = pCurr;
						pCurr = pSucc;
					} else {
						break; // move to _next iLevel
					}
				}
				if(null != preds)
					preds[iLevel] = pPred;
				if(null != succs)
					succs[iLevel] = pCurr;
			}

			if (pCurr->_key == key) // bottom iLevel curr._key == key
			{
				found_count++;
				return pCurr;
			}
			else
			{
				found_count--;
				return null;

			}
		}
	}

public://methods

	MyLFSkipList() //:
		//_num_threads (_gNumThreads)
	{
		_num_threads = _gNumThreads;
		tprng = new Random[_num_threads];

		_max_level = _MAX_LEVEL;

		_head = new Node(INT_MIN, _max_level);
		_tail = new Node(INT_MAX, _max_level);

		for (int iLevel = 0; iLevel < _head->_topLevel; ++iLevel) {
			_head->_next[iLevel].set(_tail, false);
		}

		///////////////////////////////////////////////////
		//for (int i = 0; i < _num_threads; i++) {
		//	not_found[i].val = 0;
		//	failed_cas[i].val = 0;
		//}

		found_count = 0;
	}

	MyLFSkipList(int size)
	{
		_est_size = size / 2;
		_num_threads = _gNumThreads;
		tprng = new Random[_num_threads];

		_max_level = _MAX_LEVEL;

		for (int i = 1; i < _MAX_LEVEL; i++) {		//_max_level depends on size
			if (size < (1 << i)) {
				_max_level = i;
				break;
			}
		}

		_head = new Node(INT_MIN, _max_level);
		_tail = new Node(INT_MAX, _max_level);

		for (int iLevel = 0; iLevel < _head->_topLevel; ++iLevel) {
			_head->_next[iLevel].set(_tail, false);
		}

		int node_count = 0;
		int currVal;
		Random rand;

		while (node_count < size / 2) {
			currVal = rand.nextInt(size);
			if (add(0, currVal))
				node_count++;
		}

		///////////////////////////////////////////////////
		//for (int i = 0; i < _num_threads; i++) {
		//	not_found[i].val = 0;
		//	failed_cas[i].val = 0;
		//}

		found_count = 0;
	}

	~MyLFSkipList() {
		Node* currNode = _head;
		Node* nextNode;
		int count = 0;

		while (currNode != _tail) {
			nextNode = currNode->_next[0].getReference();
			delete currNode;
			currNode = nextNode;
			count++;
		}

		delete _tail;
		if (count * 8 / 10 > _est_size || count / 8 * 10 < _est_size)
			std::cerr << " " << found_count << " found diff; " << count + 1 << " of " << _est_size << " nodes deleted in cleanup\n";

		delete[] tprng;
	}

	//enq ......................................................
	boolean add(final int iThread, final int inValue) {
		Node* preds[_max_level + 1];
		Node* succs[_max_level + 1];
		Node* pPred;
		Node* pSucc;
		Node* new_node = null;
		int topLevel = 0;

		while (true) {
			Node* final found_node = find(iThread, inValue, preds, succs);

#if 1
			// this disallows duplicates
			if(null != found_node) {
				//////////////////not_added[iThread].val++;
				return false;
			}
#endif

			// try to splice in new node in 0 going up
			pPred = preds[0];
			pSucc = succs[0];

			if (null == new_node) {
				topLevel = randomLevel(iThread);
				new_node = new Node(inValue, topLevel);
			}

			// prepare new node
			for (int iLevel = 1; iLevel < topLevel; ++iLevel) {
				new_node->_next[iLevel].set(succs[iLevel], false);
			}
			// TODO(irina): move in this in the above for loop (and start the for loop from 0)
			new_node->_next[0].set(pSucc, false);

			if (false == (pPred->_next[0].compareAndSet(pSucc, new_node, false, false))) {// lin point
				///////////////////////////failed_cas[iThread].val++;
				continue; // retry from start
			}

			// splice in remaining levels going up
			for (int iLevel = 1; iLevel < topLevel; ++iLevel) {
				while (true) {
					pPred = preds[iLevel];
					pSucc = succs[iLevel];
					if (pPred->_next[iLevel].compareAndSet(pSucc, new_node, false, false)) {
						break;
					}
					///////////////////////////failed_cas[iThread].val++;
					find(iThread, inValue, preds, succs); // find new preds and succs
				}
			}

			return true;
		}
	}


	int remove(final int iThread, final int inValue) {
		Node* succs[_max_level + 1];
		Node* preds[_max_level + 1];
		Node* pSucc;
		Node* pNodeToRemove;
		int key = inValue;

		while (true) {
			final boolean found = find(iThread, key, preds, succs);
			if (!found) { //if found it's not marked
#ifdef SKIPLIST_REMOVE_ALWAYS
				// try to remove successor
				key = succs[0]->_key;
				if (key == _tail->_key) {
					// if no successor, try to remove predecessor
					key = preds[0]->_key;
				}
				if (key == _head->_key) {
#endif
					//////////////////////////////////not_found[iThread].val++;
					return _NULL_VALUE;
#ifdef SKIPLIST_REMOVE_ALWAYS
				}
#endif
			}
			else {
				//logically remove node
				pNodeToRemove = succs[0];

				for (int iLevel = pNodeToRemove->_topLevel - 1; iLevel > 0; --iLevel) {
					boolean marked = false;
					pSucc = pNodeToRemove->_next[iLevel].get(&marked);
					while (!marked) { // until I succeed in marking
						pNodeToRemove->_next[iLevel].attemptMark(pSucc, true);
						pSucc = pNodeToRemove->_next[iLevel].get(&marked);
					}
				}

				// proceed to remove from bottom iLevel
				boolean marked = false;
				pSucc = pNodeToRemove->_next[0].get(&marked);
				while (true) { // until someone succeeded in marking
					final boolean iMarkedIt = pNodeToRemove->_next[0].compareAndSet(pSucc, pSucc, false, true);
					pSucc = succs[0]->_next[0].get(&marked);
					if (iMarkedIt) {
						// run find to remove links of the logically removed node
						findAndClean(iThread, key);
						return pNodeToRemove->_key;
					}
					else {
						/////////////////////////////failed_cas[iThread].val++;
						if (marked) {
							// repeat the big loop; this item was stolen by another thread
							// but we have duplicates, so we might be able to find another item
							break;

#if 0
							// this is correct only if we don't have duplicates in the skiplist
							not_found[iThread].val++;
							return _NULL_VALUE; // someone else removed node
#endif

							// else only pSucc changed so repeat
						}
					}
				}
			}
		}
	}



	int contain(final int iThread, final int inKey) {
		Node* pPred;
		Node* pCurr;
		Node* pSucc;

		pPred = _head;
		boolean marked = false;
		for (int iLevel = _max_level - 1; iLevel >= 0; --iLevel) {
			pCurr = pPred->_next[iLevel].getReference();
			//if(pCurr == _tail)
			//	return _NULL_VALUE;
			while (true) {
				pSucc = pCurr->_next[iLevel].get(&marked);
				while ( marked) {
					pCurr = pCurr->_next[iLevel].getReference();
					pSucc = pCurr->_next[iLevel].get(&marked);
				}

				if (inKey > pCurr->_key) { // move forward same iLevel
					pPred = pCurr;
					pCurr = pSucc;
				} else {
					break; // move to _next iLevel
				}
			}
		}
		if (inKey == pCurr->_key) // bottom iLevel curr._key == key
			return pCurr->_key;
		else return _NULL_VALUE;
	}

	//general .....................................................
	// Caution: this method is not thread-safe! To be used by a single thread only!
	int size() {
		int nf = 0;
		int na = 0;
		int failedCAS = 0;
		int _size = 0;
		int _size_nm = 0;
		boolean marked = false;
		Node *curr = _head->_next[0].get(&marked);
		while (curr != _tail) {
			_size++;
			if (!marked) _size_nm++;
			curr = curr->_next[0].get(&marked);
		}
		/////////////////////////////////////////////////
		//for (int i = 0; i < _num_threads; ++i) {
		//	nf += not_found[i].val;
		//	na += not_added[i].val;
		//	failedCAS += failed_cas[i].val;
		//}
		//printf("Failed CAS: %d\n", failedCAS);

		////printf("_size %d _size_nm %d\n", _size, _size_nm);
		////printf("Not found %d not added %d\n", nf, na);
		return _size;
	}

	final char* name() {
		return "LFSkipListSL";
	}
};



//------------------------------------------------------------------------------
// File    : LFSkipList.h
// Author  : Ms.Moran Tzafrir; email: morantza@gmail.com; phone: +972-505-779961
// Written : 13 April 2009
//
// Lock Free Skip List
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------
//*/
#endif

