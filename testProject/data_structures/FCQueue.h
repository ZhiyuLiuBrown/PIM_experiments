#ifndef __FC_QUEUE__
#define __FC_QUEUE__

////////////////////////////////////////////////////////////////////////////////
// File    : FCQueue.h
// Author  : Ms.Moran Tzafrir;  email: morantza@gmail.com; tel: 0505-779961
// Written : 27 October 2009
//
// Copyright (C) 2009 Moran Tzafrir.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of 
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License 
// along with this program; if not, write to the Free Software Foundation
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
////////////////////////////////////////////////////////////////////////////////
// TODO:
//
////////////////////////////////////////////////////////////////////////////////

#include "../framework/cpp_framework.h"
#include "ITest.h"

using namespace CCP;

class FCQueue : public ITest {
public:
	int _length;


public: //private:

	//constants -----------------------------------

	//inner classes -------------------------------
	struct Node {
		Node* volatile	_next;
		int*	volatile	_values; //was 256
		//int	volatile	_values[6]; //was 256

		static Node* get_new(final int in_num_values) {
			//dynamically allocate memory in the original code
			//final size_t new_size = (sizeof(Node) + (in_num_values + 2 - 256) * sizeof(int)); //
			//Node* final new_node = (Node*) malloc(new_size);

			Node* new_node = new Node; ////////////888888888888888

			//size_t new_size = sizeof(Node); //
			//if (in_num_values > 4)
			//	new_size = (sizeof(Node) + (in_num_values + 2 - 6) * sizeof(int)); //

			//new_node = (Node*) malloc(new_size);

			new_node->_values = new int[in_num_values + 2];////////////888888888888888

			new_node->_next = null;

			//std::cerr << sizeof(*new_node) << "\n";
			//std::cerr << sizeof(Node) << "\n";

			return new_node;
		}

		~Node() {  ////////////888888888888888
			delete[] _values;
		}

	};

	//fields --------------------------------------
	AtomicInteger	_fc_lock;
	char							_pad1[CACHE_LINE_SIZE];
	final int		_NUM_REP;
	final int		_REP_THRESHOLD;
	Node* volatile	_head;
	Node* volatile	_tail;
	int volatile	_NODE_SIZE;
	Node* volatile	_new_node;

	//helper function -----------------------------
	inline_ void flat_combining() {
		++_FCCount;
		
		// prepare for enq
		int volatile* enq_value_ary;
		if(null == _new_node) 
			_new_node = Node::get_new(_NODE_SIZE);
		enq_value_ary = _new_node->_values;
		*enq_value_ary = 1;
		++enq_value_ary;

		// prepare for deq
		int volatile * deq_value_ary = _tail->_values;
		deq_value_ary += deq_value_ary[0];

		//
		int num_added = 0;
		for (int iTry=0;iTry<_NUM_REP; ++iTry) {
			Memory::read_barrier();

			int num_changes=0;
			SlotInfo* curr_slot = _tail_slot.get();
			while(null != curr_slot->_next) {
				final int curr_value = curr_slot->_req_ans;
				if(curr_value > _NULL_VALUE) {
					++_FCCombinedCount;
					++_length;//////

					++num_changes;
					*enq_value_ary = curr_value;
					++enq_value_ary;
					curr_slot->_req_ans = _NULL_VALUE;
					curr_slot->_time_stamp = _NULL_VALUE;

					++num_added;

					if(num_added >= _NODE_SIZE) {

						//std::cerr << "need to extend node\n";

						Node* final new_node2 = Node::get_new(_NODE_SIZE+4);
						memcpy((void*)(new_node2->_values), (void*)(_new_node->_values), (_NODE_SIZE+2)*sizeof(int) );

						///////////////////////////////////////////////////////////////////////////
						//free(_new_node);///////////////////////////////////////////////////////////
						///////////////////////////////////////////////////////////////////////////
						delete(_new_node);

						_new_node = new_node2; 
						enq_value_ary = _new_node->_values;
						*enq_value_ary = 1;
						++enq_value_ary;
						enq_value_ary += _NODE_SIZE;
						_NODE_SIZE += 4;
					}
				} else if(_DEQ_VALUE == curr_value) {
					_length--;///////
					++_FCCombinedCount;///

					++num_changes;
					final int curr_deq = *deq_value_ary;
					if(0 != curr_deq) {
						curr_slot->_req_ans = -curr_deq;
						curr_slot->_time_stamp = _NULL_VALUE;
						++deq_value_ary;
					} else if(null != _tail->_next) {
						Node* volatile recycled_node = _tail;
						_tail = _tail->_next;
						deq_value_ary = _tail->_values;
						deq_value_ary += deq_value_ary[0];

						delete(recycled_node);//free dequeued node

						continue;
					} else {
						curr_slot->_req_ans = _NULL_VALUE;
						curr_slot->_time_stamp = _NULL_VALUE;
					} 
				}
				curr_slot = curr_slot->_next;
			}//while on slots

			if(num_changes < _REP_THRESHOLD)/////////////////////////////////////
				break;
		}//for repetition

		if(0 == *deq_value_ary && null != _tail->_next) {
			Node* volatile next_node = _tail->_next;///////////8888888888888888
			delete(_tail);
			_tail = next_node;
		} else {
			_tail->_values[0] = (deq_value_ary -  _tail->_values);
		}

		if(enq_value_ary != (_new_node->_values + 1)) {
			*enq_value_ary = 0;
			_head->_next = _new_node;
			_head = _new_node;
			_new_node  = null;

			//////////////////////////////////////////////////
			_NODE_SIZE = 5;////////////////////////////correct?
		}


	}

public:
	//public operations ---------------------------
	FCQueue() 
	:	//_NUM_REP(_NUM_THREADS),
		//_NUM_REP(2), ////////////////////////////////////////////////////x rounds of combining
		_NUM_REP( Math::Min(2, _NUM_THREADS)),
		_REP_THRESHOLD((int)(Math::ceil(_NUM_THREADS/(1.7))))
	{
		_head = Node::get_new(_NUM_THREADS);

		//std::cerr << sizeof(Node) << " " << sizeof(*_head)<< " " << sizeof(_head) << " " << _NUM_THREADS;

		_tail = _head;
		_head->_values[0] = 1;
		_head->_values[1] = 0;

		_tail_slot.set(new SlotInfo());
		_timestamp = 0;
		_NODE_SIZE = 5; /////////////////////////////////////////////was 4
		_new_node = null;

		_length = 0;
		_FCCount = 1;
		_FCCombinedCount = 0;
	}

	~FCQueue() {
		Node* volatile curr_node = _tail;
		int count = 0;

		while (_tail != null) {
			curr_node = _tail;
			_tail = _tail->_next;
			delete(curr_node);//free(curr_node);
			++count;
		}

		//std::cerr << count << " nodes freed\n";
	}

	//enq ......................................................
	boolean add(final int iThread, final int inValue) {
		CasInfo& my_cas_info = _cas_info_ary[iThread];

		SlotInfo* my_slot = _tls_slot_info;
		if(null == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = inValue;

		do {
			if (null == my_next)
				enq_slot(my_slot);

			boolean is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				++(my_cas_info._succ);
				//machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				//machine_end_fc(iThread);
				++(my_cas_info._ops);
				return true;
			} else {
				Memory::write_barrier();
				if(!is_cas)
					++(my_cas_info._failed);
				while(_NULL_VALUE != my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				} 
				Memory::read_barrier();
				if(_NULL_VALUE == my_re_ans) {
					++(my_cas_info._ops);
					return true;
				}
			}
		} while(true);
	}

	//deq ......................................................
	int remove(final int iThread, final int inValue) {
		CasInfo& my_cas_info = _cas_info_ary[iThread];

		SlotInfo* my_slot = _tls_slot_info;
		if(null == my_slot)
			my_slot = get_new_slot();

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = _DEQ_VALUE;

		do {
			if(null == my_next)
				enq_slot(my_slot);

			boolean is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				++(my_cas_info._succ);
				//machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				//machine_end_fc(iThread);
				++(my_cas_info._ops);
				return -(my_re_ans);
			} else {
				Memory::write_barrier();
				if(!is_cas)
					++(my_cas_info._failed);
				while(_DEQ_VALUE == my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				}
				Memory::read_barrier();
				if(_DEQ_VALUE != my_re_ans) {
					++(my_cas_info._ops);
					return -(my_re_ans);
				}
			}
		} while(true);
	}

	//peek .....................................................
	int contain(final int iThread, final int inValue) {
		return _NULL_VALUE;
	}

	//general .....................................................
	int size() {
		return 0;
	}

	final char* name() {
		return "FCQueue";
	}

};

////////////////////////////////////////////////////////////////////////////////
// File    : FCQueue.h
// Author  : Ms.Moran Tzafrir;  email: morantza@gmail.com; tel: 0505-779961
// Written : 27 October 2009
//
// Copyright (C) 2009 Moran Tzafrir.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of 
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License 
// along with this program; if not, write to the Free Software Foundation
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
////////////////////////////////////////////////////////////////////////////////

#endif
