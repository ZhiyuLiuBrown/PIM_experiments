/*
 * MyFCLinkedList.h
 *
 *  Created on: Jul 8, 2016
 *      Author: zhiyul
 */

#ifndef MYFCLINKEDLIST_H_
#define MYFCLINKEDLIST_H_

#include "../framework/cpp_framework.h"

#include <iostream>
#include <vector>
#include <algorithm>

using namespace CCP;


class MyFCLinkedList : public ITest {
protected://consts
	static final int _STAMP_THRESHOLD = 25; //used to decide if a slot should be removed from the list

protected://types

	class Node {
	public:
		char padding [128 - sizeof(int) - sizeof(Node*) - sizeof(TASLock) - sizeof(volatile bool) - 8]; //why is -8 needed?
		int		_key;
		Node* 	_next;//VolatileType<Node*> 	_next;
		TASLock	_lock;//for the parallel lazy algorithm
		volatile bool _marked; //for the parallel lazy algorithm


	public:

		static Node* getNewNode(final int inKey) {
			Node* final new_node = (Node*) malloc(sizeof(Node));
			new_node->_key	= inKey;
			new_node->_next = null; //added line for safety
			new_node->_marked = false;
			new_node->_lock.init();

			return new_node;
		}
	};

protected://SkipList fields
	VolatileType<_u64>	_random_seed;
	Node*	final		_head;

protected://Flat Combining fields

	AtomicInteger	_fc_lock;
	char							_pad1[CACHE_LINE_SIZE];
	//SlotInfo*		_saved_remove_node[1024];


public://methods

	static bool nodeCompare (Node* a, Node* b) {
			return a->_key < b->_key;
		}

	MyFCLinkedList(int num_intialNodes)
	: _head( Node::getNewNode(INT_MIN) )
	{
		_FCCount = 0; //extra count
		_FCCombinedCount = 0; //extra count

		//initialize the slots .........................................
		_tail_slot.set(null); // _tail_slot.set(new SlotInfo());
		_timestamp = 0;
		_tls_slot_info = null;

		//for (int i = 0; i < _gNumThreads; i++)
		//	_tls_slot_info_part[i] = null;

		Random rng;

		Node** node_list = new Node*[num_intialNodes];

		for (int i = 0; i < num_intialNodes; i++) {
			node_list[i] = Node::getNewNode(rng.nextInt(num_intialNodes));
		}

		sort(node_list, node_list + num_intialNodes, nodeCompare);

		Node* curr_node = _head;
		for (int i = 0; i < num_intialNodes; i++) {
				if (curr_node->_key < node_list[i]->_key) {
					curr_node->_next = node_list[i];
					curr_node = node_list[i];
				}
				else
					free(node_list[i]);
			}

		curr_node->_next = Node::getNewNode(INT_MAX);

		delete[] node_list;
	}

	MyFCLinkedList(int lbound, int rbound) :
			_head(Node::getNewNode(INT_MIN)) {
		_FCCount = 0; //extra count
		_FCCombinedCount = 0; //extra count

		//initialize the slots .........................................
		_tail_slot.set(null); // _tail_slot.set(new SlotInfo());
		_tls_slot_info = null;
		_timestamp = 0;

		//for (int i = 0; i < _gNumThreads; i++)
		//			_tls_slot_info_part[i] = null;

		Random rng;

		int num_intialNodes = rbound - lbound + 1;

		Node** node_list = new Node*[num_intialNodes];

		for (int i = 0; i < num_intialNodes; i++) {
			node_list[i] = Node::getNewNode(rng.nextInt(num_intialNodes) + lbound);
		}

		sort(node_list, node_list + num_intialNodes, nodeCompare);

		Node* curr_node = _head;
		for (int i = 0; i < num_intialNodes; i++) {
			if (curr_node->_key < node_list[i]->_key) {
				curr_node->_next = node_list[i];
				curr_node = node_list[i];
			} else
				free(node_list[i]);
		}

		curr_node->_next = Node::getNewNode(INT_MAX);

		delete[] node_list;
	}

	MyFCLinkedList()
	: _head( Node::getNewNode(INT_MIN) )
	{
		_head->_next = Node::getNewNode(INT_MAX);
		//initialize the slots .........................................
		_tail_slot.set(null); // _tail_slot.set(new SlotInfo());
		_timestamp = 0;
		_tls_slot_info = null;

		//for (int i = 0; i < _gNumThreads; i++)
		//			_tls_slot_info_part[i] = null;
	}

	~MyFCLinkedList() {
		Node* volatile curr_node = _head;
		Node* volatile next_node;
		int count = 0;

		while (curr_node != null) {
			next_node = curr_node->_next;
			free(curr_node);
			++count;
			curr_node = next_node;
		}

		//std::cerr << count << " nodes freed\n";
	}

	int node_count () {
		Node* curr_node = _head;
		int count = 0;

		while (curr_node != null) {
			count++;
			curr_node = curr_node->_next;

			//if (count < 10)
			//	std::cerr << curr_node->_key << " at address " << curr_node << "\n";
		}

		return count;
	}


	inline_ void flat_combining() {

		_timestamp++;
		int curr_stamp = _timestamp;

		if (curr_stamp > 1000000000)
			std::cerr << "Exceeds max iteration number. Kill the program now!!!! \n";

		//std::cerr << "New flat combining round \n";

		SlotInfo* curr_slot = _tail_slot.get();
		SlotInfo* prev_slot = curr_slot;

		Memory::read_barrier();

		_FCCount++; // extra count

		while(null != curr_slot) {
			if(curr_slot->_is_request_valid) { //execute the operation

				_FCCombinedCount++; // extra count

				final int op = curr_slot->_op;
				final int inValue = curr_slot->_req_ans;

				/////////////////////////////////////
				//execute the operation
				if (op == _OP_CONTAIN) {
					//std::cerr << "enter correct if condition\n";
					curr_slot->_return_val = _ANS_FALSE;
					Node* curr_node = _head;

					while (curr_node->_key < inValue) {
						curr_node = curr_node->_next;
					}

					if (curr_node->_key == inValue) {
						curr_slot->_return_val = _ANS_TRUE;
					}
				}

				else if (op == _OP_ADD) {
					//std::cerr << "enter wrong if condition\n";
					curr_slot->_return_val = _ANS_FALSE;
					Node* curr_node = _head;
					Node* prev_node = _head;

					while (curr_node->_key < inValue) {
						prev_node = curr_node;
						curr_node = curr_node->_next;
					}

					if (curr_node->_key != inValue ) {
						Node* newNode = Node::getNewNode(inValue);
						prev_node->_next = newNode;
						newNode->_next = curr_node;

						curr_slot->_return_val = _ANS_TRUE;

					}
				}

				else { // if (op == _OP_DEL)
					//std::cerr << "enter wrong if condition\n";
					curr_slot->_return_val = _ANS_FALSE;
					Node* curr_node = _head;
					Node* prev_node = _head;

					while (curr_node->_key < inValue) {
						prev_node = curr_node;
						curr_node = curr_node->_next;
					}

					if (curr_node->_key == inValue) {
						prev_node->_next =  curr_node->_next;
						free(curr_node);
						curr_slot->_return_val = _ANS_TRUE;
					}
				}
				///////
				/////////////////////////////////////
				curr_slot->_time_stamp = curr_stamp;
				curr_slot->_is_request_valid = false;

				prev_slot = curr_slot;
				curr_slot = curr_slot->_next;
			}

			else { //remove the slot from the list if necessary
				if (curr_slot != prev_slot &&								//not the tail
					curr_stamp - curr_slot->_time_stamp > _STAMP_THRESHOLD) //unused for long
				{
					//std::cerr << "removing a slot! \n";

					prev_slot->_next = curr_slot->_next;
					curr_slot->_is_slot_linked = false;
					curr_slot = prev_slot->_next;

				}

				else {
					prev_slot = curr_slot;
					curr_slot = curr_slot->_next;
				}
			}

		}

	}


	static bool requestsCompare (SlotInfo* a, SlotInfo* b) {
		return a->_req_ans < b->_req_ans;
	}

	inline_ void flat_combining_combined() {

		_timestamp++;
		int curr_stamp = _timestamp;

		if (curr_stamp > 1000000000)
			std::cerr
					<< "Exceeds max iteration number. Kill the program now!!!! \n";

		//std::cerr << "New flat combining round \n";

		SlotInfo* curr_slot = _tail_slot.get();
		SlotInfo* prev_slot = curr_slot;

		std::vector<SlotInfo*> requests;

		Memory::read_barrier();

		_FCCount++; //extra count

		while (null != curr_slot) {
			if (curr_slot->_is_request_valid) { //execute the operation

				_FCCombinedCount++; // extra count

				requests.push_back(curr_slot);

				prev_slot = curr_slot;
				curr_slot = curr_slot->_next;

			}

			else { //remove the slot from the list if necessary
				if (curr_slot != prev_slot &&					//not the tail
						curr_stamp - curr_slot->_time_stamp > _STAMP_THRESHOLD) //unused for long
								{
					//std::cerr << "removing a slot! \n";

					prev_slot->_next = curr_slot->_next;
					curr_slot->_is_slot_linked = false;
					curr_slot = prev_slot->_next;

				}

				else {
					prev_slot = curr_slot;
					curr_slot = curr_slot->_next;
				}
			}

		}

		//sort requests
		std::sort(requests.begin(), requests.end(), requestsCompare);

		/////////////////////////////////////
		//search
		Node* curr_node = _head;
		Node* prev_node = _head;

		for (std::vector<SlotInfo*>::iterator iter = requests.begin(); iter != requests.end(); ++iter) {
			(*iter)->_return_val = _ANS_FALSE;
			int inValue = (*iter)->_req_ans;
			int op = (*iter)->_op;

			while (curr_node->_key < inValue) {
				prev_node = curr_node;
				curr_node = curr_node->_next;
			}

			if (op == _OP_CONTAIN) {
				if (curr_node->_key == inValue) {
					(*iter)->_return_val = _ANS_TRUE;
				}
			}
			else if (op == _OP_ADD) {
				if (curr_node->_key != inValue) {
					Node* newNode = Node::getNewNode(inValue);
					prev_node->_next = newNode;
					newNode->_next = curr_node;
					curr_node = newNode;
					(*iter)->_return_val = _ANS_TRUE;

				}
			}
			else {	// op == _OP_DEL
				if (curr_node->_key == inValue) {
					prev_node->_next = curr_node->_next;
					free(curr_node);
					curr_node = prev_node->_next;
					(*iter)->_return_val = _ANS_TRUE;

				}
			}

			(*iter)->_time_stamp = curr_stamp;
			(*iter)->_is_request_valid = false;
		}
		///////
		/////////////////////////////////////

	}



public://methods

	int contain(final int iThread, final int inValue){return 0;};
	boolean add(final int iThread, final int inValue){return true;};
	int remove(final int iThread, final int inValue){return 0;};
	/*
	//insert ......................................................
	boolean add(final int iThread, final int inValue) {
		SlotInfo* my_slot = _tls_slot_info;
		if(null == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = inValue;

		do {
			if (null == my_next)
				enq_slot(my_slot);

			boolean is_cas = false;
			if(lock_fc(_fc_lock, is_cas)) {
				machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				machine_end_fc(iThread);
				return true;
			} else {
				Memory::write_barrier();
				while(_NULL_VALUE != my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				}
				Memory::read_barrier();
				if(_NULL_VALUE == my_re_ans) {
					return true;
				}
			}
		} while(true);
	}

	//delete ......................................................
	int remove(final int iThread, final int inValue) {
		SlotInfo* my_slot = _tls_slot_info;
		if(null == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = _DEQ_VALUE;

		do {
			if(null == my_next)
				enq_slot(my_slot);

			boolean is_cas = false;
			if(lock_fc(_fc_lock, is_cas)) {
				machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				machine_end_fc(iThread);
				return -(my_re_ans);
			} else {
				Memory::write_barrier();
				while(_DEQ_VALUE == my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				}
				Memory::read_barrier();
				if(_DEQ_VALUE != my_re_ans) {
					return -(my_re_ans);
				}
			}
		} while(true);
	}
	//*/

	inline_ int singleFCOP (final int iThread, final int inValue, final int op, SlotInfo* local_slot) {
		SlotInfo* my_slot = local_slot;

		my_slot->_op = op;
		my_slot->_req_ans = inValue;
		my_slot->_is_request_valid = true;

		if (!my_slot->_is_slot_linked && my_slot->_is_request_valid)
			enq_slot(my_slot);

		do {
			boolean is_cas = false;
			if (lock_fc(_fc_lock, is_cas)) {

				if (!my_slot->_is_slot_linked && my_slot->_is_request_valid)
					enq_slot(my_slot);

				machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				machine_end_fc(iThread);
				return my_slot->_return_val;
			} else {
				Memory::write_barrier();

				while (my_slot->_is_request_valid && 0 != _fc_lock.getNotSafe()) { //
					thread_wait(iThread);

					if (!my_slot->_is_slot_linked && my_slot->_is_request_valid)
						enq_slot(my_slot);
				}
				Memory::read_barrier();

				if (!my_slot->_is_request_valid) {
					return my_slot->_return_val;
				}
			}
		} while (true);
	}


	//OPs using single FC......................................................................
	int singleFCContain(final int iThread, final int inValue, SlotInfo* local_slot) {
		return singleFCOP(iThread, inValue, _OP_CONTAIN, local_slot);
	}

	int singleFCAdd(final int iThread, final int inValue, SlotInfo* local_slot) {
		return singleFCOP(iThread, inValue, _OP_ADD, local_slot);
	}

	int singleFCDelete(final int iThread, final int inValue, SlotInfo* local_slot) {
		return singleFCOP(iThread, inValue, _OP_DEL, local_slot);
	}

	inline_ int combinedFCOP(final int iThread, final int inValue, final int op, SlotInfo* local_slot) {
		SlotInfo* my_slot = local_slot;

		my_slot->_op = op; //e.g. _OP_CONTAIN;
		my_slot->_req_ans = inValue;
		my_slot->_is_request_valid = true;

		if (!my_slot->_is_slot_linked && my_slot->_is_request_valid)
			enq_slot(my_slot);

		do {
			boolean is_cas = false;
			if (lock_fc(_fc_lock, is_cas)) {

				if (!my_slot->_is_slot_linked && my_slot->_is_request_valid)
					enq_slot(my_slot);

				machine_start_fc(iThread);
				flat_combining_combined();
				_fc_lock.set(0);
				machine_end_fc(iThread);

				return my_slot->_return_val;
			} else {
				Memory::write_barrier();

				while (my_slot->_is_request_valid && 0 != _fc_lock.getNotSafe()) { //
					thread_wait(iThread);

					if (!my_slot->_is_slot_linked && my_slot->_is_request_valid)
						enq_slot(my_slot);
				}
				Memory::read_barrier();

				if (!my_slot->_is_request_valid) {
					return my_slot->_return_val;
				}
			}
		} while (true);
	}

	//OPs using combined FC......................................................................
	int combinedFCContain(final int iThread, final int inValue, SlotInfo* local_slot) {
		return combinedFCOP(iThread, inValue, _OP_CONTAIN, local_slot);
	}

	int combinedFCAdd(final int iThread, final int inValue, SlotInfo* local_slot) {
		return combinedFCOP(iThread, inValue, _OP_ADD, local_slot);
	}

	int combinedFCDelete(final int iThread, final int inValue, SlotInfo* local_slot) {
		return combinedFCOP(iThread, inValue, _OP_DEL, local_slot);
	}

	//parallel OPs
	bool parallelContain(final int iThread, final int inValue) {
		Node* curr_node = _head;

		while (curr_node->_key < inValue) {
			curr_node = curr_node->_next;
		}

		if (curr_node->_key == inValue && !curr_node->_marked)
			return true;
		else
			return false;
	}

	bool parallelAdd(final int iThread, final int inValue) {
		bool return_val = false;

		while (true) {
			Node* prev_node = _head;
			Node* curr_node = _head->_next;

			while (curr_node->_key < inValue) {
				prev_node = curr_node;
				curr_node = curr_node->_next;
			}

			prev_node->_lock.lock();
			curr_node->_lock.lock();

			if (!prev_node->_marked && !curr_node->_marked
					&& prev_node->_next == curr_node) { //if valid
				if (curr_node->_key != inValue) {
					Node* newNode = Node::getNewNode(inValue);
					newNode->_next = curr_node;

					//Memory::read_write_barrier();

					prev_node->_next = newNode;
					return_val = true;
				}

				curr_node->_lock.unlock();
				prev_node->_lock.unlock();
				return return_val;
			}

			curr_node->_lock.unlock();
			prev_node->_lock.unlock();
		}
	}

	bool parallelDelete(final int iThread, final int inValue) {
		bool return_val = false;

		while (true) {
			Node* prev_node = _head;
			Node* curr_node = _head->_next;

			while (curr_node->_key < inValue) {
				prev_node = curr_node;
				curr_node = curr_node->_next;
			}

			prev_node->_lock.lock();
			curr_node->_lock.lock();

			if (!prev_node->_marked && !curr_node->_marked && prev_node->_next == curr_node) { //if valid
				if (curr_node->_key == inValue) {
					curr_node->_marked = true;

					//Memory::read_write_barrier();

					prev_node->_next = curr_node->_next;
					return_val = true;
				}

				curr_node->_lock.unlock();
				prev_node->_lock.unlock();
				return return_val;
			}

			curr_node->_lock.unlock();
			prev_node->_lock.unlock();

		}

	}

public:
	//methods

	int size() {
		return 0;
	}

	final char* name() {
		return "MyFCLinkedList";
	}

};



#endif /* MYFCLINKEDLIST_H_ */
