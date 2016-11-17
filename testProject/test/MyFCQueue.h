#ifndef __MY_FC_QUEUE__
#define __MY_FC_QUEUE__

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


using namespace CCP;

class MyFCQueue : public ITest {
public:
	int volatile _length;

	//node for queue
	struct QNode {
		QNode* volatile _next;
		RNode* _rNode;

		QNode() :
				_rNode(null), _next(null) {
		}

		QNode(RNode* rNode) :
						_rNode(rNode), _next(null) {
				}

		~QNode() {
		}
	};

public: //private:

	//constants -----------------------------------

	//fields --------------------------------------
	AtomicInteger	_fc_lock;
	QNode* volatile	_head;
	QNode* volatile	_tail;
	QNode* volatile	_new_node;

	//helper function -----------------------------
	inline_ void flat_combining() {
		++_FCCount;

		SlotInfo* curr_slot = _tail_slot.get();

		while (null != curr_slot->_next) {
			final int curr_value = curr_slot->_req_ans;

			if (curr_value > _NULL_VALUE) { //enq
				++_length;
				++_FCCombinedCount;

				_head->_next = new QNode(curr_slot->_rNode);
				_head = _head->_next;

				curr_slot->_req_ans = _NULL_VALUE;
				//curr_slot->_time_stamp = _NULL_VALUE;


			}
			else if (_DEQ_VALUE == curr_value) { //deq
				if (null != _tail->_next) {
					--_length;
					++_FCCombinedCount;

					curr_slot->_rNode = _tail->_next->_rNode;
					QNode* volatile currNode = _tail;
					_tail = _tail->_next;
					delete(currNode);
					curr_slot->_req_ans = _NULL_VALUE;
				}
				else {
					curr_slot->_rNode = null;
					curr_slot->_req_ans = _NULL_VALUE;
					//curr_slot->_time_stamp = _NULL_VALUE;
				}
			}

			curr_slot = curr_slot->_next;
		}						//while on slots
	}

public:
	//public operations ---------------------------
	MyFCQueue()
	{
		_head = new QNode();

		_tail = _head;
		_tail_slot.set(new SlotInfo());
		_timestamp = 0;

		_new_node = null;

		_length = 0;

	}

	~MyFCQueue() {
		QNode* volatile curr_node = _tail;
		int count = 0;

		while (_tail->_next != null) {
			curr_node = _tail;
			_tail = _tail->_next;
			delete(curr_node);
			delete(_tail->_rNode);
			++count;
		}

		delete(_tail);

		//std::cerr << count << " nodes freed\n";
	}

	//enq ......................................................
	boolean add(final int iThread, RNode* node) {
		//////////CasInfo& my_cas_info = _cas_info_ary[iThread];

		SlotInfo* my_slot = _tls_slot_info;
		if(null == my_slot)
			my_slot = get_new_slot();

		my_slot->_rNode = node;///////////////

		SlotInfo* volatile&	my_next = my_slot->_next;
		int volatile& my_re_ans = my_slot->_req_ans;
		my_re_ans = 1;// need > 0. was inValue;

		do {
			if (null == my_next)
				enq_slot(my_slot);

			boolean is_cas = true;
			if(lock_fc(_fc_lock, is_cas)) {
				//////////++(my_cas_info._succ);
				//machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				//machine_end_fc(iThread);
				//////////++(my_cas_info._ops);
				return true;
			} else {
				Memory::write_barrier();
				////////////if(!is_cas)
					////////////++(my_cas_info._failed);
				while(_NULL_VALUE != my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				}
				Memory::read_barrier();
				if(_NULL_VALUE == my_re_ans) {
					/////////////++(my_cas_info._ops);
					return true;
				}
			}
		} while(true);
	}

	//deq ......................................................
	RNode* remove(final int iThread) {
		//////////////CasInfo& my_cas_info = _cas_info_ary[iThread];

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
				///////////////++(my_cas_info._succ);
				//machine_start_fc(iThread);
				flat_combining();
				_fc_lock.set(0);
				//machine_end_fc(iThread);
				///////////////++(my_cas_info._ops);
				return my_slot->_rNode;
			} else {
				Memory::write_barrier();
				////////////if(!is_cas)
					/////////////////++(my_cas_info._failed);
				while(_DEQ_VALUE == my_re_ans && 0 != _fc_lock.getNotSafe()) {
					thread_wait(iThread);
				}
				Memory::read_barrier();
				if(_DEQ_VALUE != my_re_ans) {
					/////////////////++(my_cas_info._ops);
					return my_slot->_rNode;
				}
			}
		} while(true);
	}


	boolean add(final int iThread, final int inValue) {
		return _NULL_VALUE;
	}

	int remove(final int iThread, final int inValue) {
		return _NULL_VALUE;
	}

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

#endif
