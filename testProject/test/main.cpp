////////////////////////////////////////////////////////////////////////////////
// File    : main.cpp
// Author  : Ms.Moran Tzafrir  email: morantza@gmail.com
// Written : 13 October 2009
// 
// Multi-Platform C++ framework example benchamrk programm
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

////////////////////////////////////////////////////////////////////////////////
//INCLUDE DIRECTIVES
////////////////////////////////////////////////////////////////////////////////
#include <string>
#include <iostream>

//general includes .....................................
#include "../framework/cpp_framework.h"
#include "Configuration.h"
#include <pthread.h>

/////////////////////////////////////////////////////////////////
//for checking NUMA nodes
#include <utmpx.h>
//#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

//#include <linux/getcpu.h>

#include <dlfcn.h>
#include <asm/vsyscall.h>
#include <sys/auxv.h>

#include <numa.h>
//////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
/////some simple experiments
////////////////////////////////////////////////////////////////

#include "../data_structures/FCQueue.h"
#include "../data_structures/MSQueue.h"
#include "../data_structures/BasketQueue.h"
#include "../data_structures/LFStack.h"
#include "../data_structures/FCStack.h"
#include "../data_structures/LazySkipList.h"
#include "../data_structures/OyamaQueue.h"
#include "../data_structures/OyamaQueueCom.h"
#include "../data_structures/ComTreeQueue.h"
#include "../data_structures/EliminationStack.h"
#include "../data_structures/LFSkipList.h"
#include "../data_structures/FCPairingHeap.h"
#include "../data_structures/FCSkipList.h"
#include "MyFCSkipList.h"
#include "MyLFSkipList.h"
using namespace std;

static inline bool getcpu(int* cpu, int* node) {
#ifdef SYS_getcpu
	syscall(SYS_getcpu, cpu, node, NULL);
	return true;
#else
	return false; // unavailable
#endif
}

//*skiplist tests
////////////////////////////////////////////////////////////////////////////////
//CONSTS
////////////////////////////////////////////////////////////////////////////////
#include "MyFCLinkedList.h"

static final int _ACTIONS_ARY_SIZE = 2 * 1024 * 1024;

static int _num_ds;
static ITest* _gDS[1024];
static Configuration _gConfiguration;

static Random _gRand;
static int _gActionAry[_ACTIONS_ARY_SIZE];
static int _gTotalRandNum;

static int _g_thread_fill_table_size;
static int* _gRandNumAry;
static tick_t volatile * _gThreadResultAry;

static int _gNumProcessors;
int _gNumThreads;
static int _gThroughputTime;

static Thread** _gThreads;
static AtomicInteger _gThreadStartCounter(0);
static AtomicInteger _gThreadEndCounter(0);
static VolatileType<tick_t> _gStartTime(0UL);
static VolatileType<tick_t> _gEndTime(0UL);
static VolatileType<int> _gIsStopThreads(0UL);

static tick_t _gResult = 0L;
static tick_t _gResultAdd = 0L;
static tick_t _gResultRemove = 0L;
static tick_t _gResultPeek = 0L;

static _u64 volatile _seed;
static boolean _is_tm = false;
static boolean _is_view = false;

static ITest* _myDS; //
static int _initial_size; //
static volatile int _executionMode; //
static int _nanosec; //
static AtomicInteger _totalOP(0); //
static AtomicInteger _initialOP(0); //
static volatile int _num_partitions = 1; //
static ITest** _myDS_part;//



////////////////////////////////////////////////////////////////////////////////
//TYPEDEFS
////////////////////////////////////////////////////////////////////////////////
class MixThread: public Thread {
public:
	final int _threadNo;
	int _totalNum;
	int _trueNum;
	int _falseNum;
	int _evenNum;
	int _oddNum;
	Random _local_rand;

	int _op_mod;//
	bool _unbalanced_distribution;//
	int _concentration_rate;//

	MixThread(final int inThreadNo) :
		_threadNo(inThreadNo), _totalNum(0), _trueNum(0), _falseNum(0), _evenNum(0), _oddNum(0) {

		_op_mod = 3;//
		_unbalanced_distribution = true;//
		_concentration_rate = 8;//
	}

	inline_
	void threadPause(int nanosec) {
		if (nanosec > 0) {
			struct timespec time, time2;
			time.tv_nsec = nanosec;
			time.tv_sec = 0;
			nanosleep(&time, &time2);
		}
	}

	inline_ int unbalanced_key (int val) {
		return val / _concentration_rate;
	}

	void run() {

		int cpuInfo;
		int nodeInfo;

#if 0
		threadPause(_threadNo * 1000000);
		if (getcpu(&cpuInfo, &nodeInfo))
			std::cerr << cpuInfo << " " << nodeInfo << " " << sched_getcpu()
					<< "\n";
		else
			std::cerr << "!!!\n";
#endif

		int currVal;

		int part_size = _initial_size / _num_partitions;

		ITest::SlotInfo* volatile local_slot = new ITest::SlotInfo();

		ITest::SlotInfo* volatile local_slots[_num_partitions];

		for (int i = 0; i < _num_partitions; i++)
			local_slots[i] = new ITest::SlotInfo();

		if (_executionMode == 0) { //single FC
			do {
				//threadPause(_nanosec);

				_totalNum++;
				currVal = _local_rand.nextInt(_initial_size);

				//unbalanced distribution
				if (_unbalanced_distribution && (_totalNum + currVal) % 2 == 0)
					currVal = unbalanced_key(currVal);

				if (_totalNum % _op_mod == 0)
					((MyFCSkipList*) _myDS)->fcAdd(_threadNo, currVal, local_slot);
				else if (_totalNum % _op_mod == 1)
					((MyFCSkipList*) _myDS)->fcDelete(_threadNo, currVal, local_slot);
				else
					((MyFCSkipList*) _myDS)->fcContain(_threadNo, currVal, local_slot);

				if (0 != _gIsStopThreads) {
					break;
				}

			} while (true);
		}

		if (_executionMode == 1) { //Look-free parallel

			do {
				//threadPause(_nanosec);

				_totalNum++;

				currVal = _local_rand.nextInt(_initial_size);

				//unbalanced distribution
				if (_unbalanced_distribution && (_totalNum + currVal) % 2 == 0)
					currVal = unbalanced_key(currVal);


				if (_totalNum % _op_mod == 0) {
					((MyLFSkipList*) _myDS)->add(_threadNo, currVal);
				}
				else if (_totalNum % _op_mod == 1) {
					((MyLFSkipList*) _myDS)->remove(_threadNo, currVal);
				}
				else {
					((MyLFSkipList*) _myDS)->contain(_threadNo, currVal);
				}

				if (0 != _gIsStopThreads) {
					break;
				}

			} while (true);
		}

		if (_executionMode >= 2) { //FC with partitioning
			do {
				//threadPause(_nanosec);
				_totalNum++;
				currVal = _local_rand.nextInt(_initial_size);

				//unbalanced distribution
				if (_unbalanced_distribution && (_totalNum + currVal) % 2 == 0)
					currVal = unbalanced_key(currVal);

				int part_id = currVal / part_size;

				if (_totalNum % _op_mod == 0)
					((MyFCSkipList*) _myDS_part[part_id])->fcAdd(_threadNo, currVal, local_slots[part_id]);
				else if (_totalNum % _op_mod == 1)
					((MyFCSkipList*) _myDS_part[part_id])->fcDelete(_threadNo, currVal, local_slots[part_id]);
				else
					((MyFCSkipList*) _myDS_part[part_id])->fcContain(_threadNo, currVal, local_slots[part_id]);

				if (0 != _gIsStopThreads) {
					break;
				}

			} while (true);
		}

		sleep(_local_rand.nextInt(2000));
		//std::cerr << "Thread " << _threadNo << " #requests: " << _totalNum
		//		<< ", #true's: " << _trueNum << "/" << _evenNum <<
		//		", #false's: " << _falseNum << "/" << _oddNum << "\n";

		//std::cerr << "Thread " << _threadNo << " #requests: " << _totalNum << "\n";
		_totalOP.getAndAdd(_totalNum);

		delete local_slot;

		for (int i = 0; i < _num_partitions; i++)
			delete local_slots[i];

	}
};

//void singleFCExecution();
//void sequetialExecution();
//void multiFCExecution();

int main(int argc, char **argv) {

	//contaminate memory manager ...............................................
	for (int i = 0; i < (1024 * 64); ++i) {
		void* final rand_mem = malloc(_gRand.nextInt(128) + 1);
		free(rand_mem);
	}

	int results[5][10][65];

	for (int j = 0; j < 5; j++)
		for (int i = 0; i < 10; i++)
			for (int numProcs = 1; numProcs <= 64; numProcs++)
				results[j][i][numProcs] = 0;

	int num_iter = 4;
	_num_partitions = 1;
	int j_val = 1;

	string algs[6] = { "single FC", "lock free", "FC with 2 partitions", "FC with 4 partitions", "FC with 8 partitions", "FC with 16 partitions" };

	for (int iter = 0; iter < num_iter; iter++) {
		if (argc <= 1)
			std::cerr << "*******************start round " << iter << "*****************\n";
		for (int j = 1; j <= 1; j++) //0-2
#if 1
			for (int i = 3; i <= 5; i = i + 1) //0-5; #partitions: 2->2, 3->4, 4->8, 5->16
				for (int numProcs = 1; numProcs <= 28; numProcs = numProcs + 3) {
#else
				for (int i = 1; i <= 1; i = i + 1)
					for (int numProcs = 1; numProcs <= 1; numProcs = numProcs + 3) {
#endif

#if 1
					if (i == 1) {
						i = 2;
					}
#else
					if (iter == 0) {
						iter = 1;
					}
#endif

					if (argc > 1) {
						numProcs = atoi(argv[1]);
						j = j_val;
					}


					//initialize global variables ..............................................
					_gNumProcessors = numProcs; //Runtime.getRuntime().availableProcessors();
					_gNumThreads = _gNumProcessors * 1;
					//_gTotalRandNum      = Math::Max(_gConfiguration._capacity, 4*1024*1024);
					_gThroughputTime = 5; //seconds
					_gIsStopThreads = 0;
					_nanosec = 0;
					_totalOP.set(0);

					if (argc <= 1)
						cout << "\n===========================#threads: " << _gNumThreads << "; algorithm: " << algs[i] << endl;

					if (j == 0)
						_initial_size = 1000000;
					if (j == 1)
						_initial_size = 4000000;
					if (j == 2)
						_initial_size = 10000000;

					_executionMode = i;

					if (_executionMode == 0)
						_myDS = new MyFCSkipList(0, _initial_size - 1);

					if (_executionMode == 1)
						_myDS = new MyLFSkipList(_initial_size);

					if (_executionMode >= 2) {
						_num_partitions = pow(2, i - 1);
						_myDS_part = new ITest*[_num_partitions];
						int part_size = _initial_size / _num_partitions;

						for (int ii = 0; ii < _num_partitions; ii++)
							_myDS_part[ii] = new MyFCSkipList(ii * part_size, (ii + 1) * part_size - 1);
					}

					Memory::read_write_barrier();

					//std::cerr << "#nodes before a run: " << ((MyFCLinkedList*) _myDS)->node_count() << " with size " << _list_size << "\n";

					//calculate how much each thread should add the data-structure initially ...
					//int table_size  = (int)((_gConfiguration._capacity) * (_gConfiguration._load_factor));
					//_g_thread_fill_table_size = table_size / _gNumThreads;

					//create benchmark threads .................................................
					//System_err_println("    START creating threads.");
					_gThreads = new Thread*[_gNumThreads];
					//_gThreadResultAry       =  new tick_t[_gNumThreads];
					//memset((void*)_gThreadResultAry, 0, sizeof(int)*_gNumThreads);

					cpu_set_t cpuset;
					int s;

					for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
						_gThreads[iThread] = new MixThread(iThread);
#if 1
						//node 0: 0, 4, 8...; node 1: 1, 5, 9...; node 2:...
						int coreNo = (iThread % 28) * 4 + (iThread / 28);

						CPU_ZERO(&cpuset);
						CPU_SET(coreNo, &cpuset);

						s = pthread_setaffinity_np(_gThreads[iThread]->_handle,
								sizeof(cpu_set_t), &cpuset);
						if (s != 0)
							std::cerr << "error: pthread_setaffinity_np\n";

						s = pthread_getaffinity_np(_gThreads[iThread]->_handle,
								sizeof(cpu_set_t), &cpuset);
						if (s != 0)
							std::cerr << "error 2: pthread_setaffinity_np\n";
#else
						CPU_ZERO(&cpuset);
						CPU_SET(iThread, &cpuset);

						s = pthread_setaffinity_np(_gThreads[iThread]->_handle,
								sizeof(cpu_set_t), &cpuset);
						if (s != 0)
							std::cerr << "error: pthread_setaffinity_np\n";

						s = pthread_getaffinity_np(_gThreads[iThread]->_handle,
								sizeof(cpu_set_t), &cpuset);
						if (s != 0)
							std::cerr << "error 2: pthread_setaffinity_np\n";
#endif
					}

					//System_err_println("    END   creating threads.");
					//System_err_println("");
					Thread::yield();

					//start the benchmark threads ..............................................
					//System_err_println("    START threads.");
					for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
						_gThreads[iThread]->start();
					}

					//System_err_println("");

					//wait the throughput time, and then signal the threads to terminate ...
					Thread::yield();

					//int threshold = 1;
					//Memory::read_write_barrier();

					Memory::read_write_barrier();
					Thread::yield();
					Thread::sleep(_gThroughputTime * 1000);

					_gIsStopThreads = 1;

					//join the threads .........................................................
					for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
						Thread::yield();
						Thread::yield();
						_gThreads[iThread]->join();
					}

					if (iter != 0)
						results[j][i][numProcs] = results[j][i][numProcs] + _totalOP.get();

					if (argc <= 1)
						std::cerr << "***total #operations: " << _totalOP.get() << "\n";

						int num_combined_reqs;
					for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
						delete _gThreads[iThread];
					}

					delete[] _gThreads;
					_gThreads = null;

					if (_executionMode < 2) {
						delete _myDS;
					}

					else {
						for (int ii = 0; ii < _num_partitions; ii++)
							delete _myDS_part[ii];
						delete[] _myDS_part;
					}

					Thread::sleep(1000);
				}
	}

if (argc <= 1) {
#if 1
	for (int j = 0; j <= 2; j++)
		for (int i = 0; i <= 5; i++)
			for (int numProcs = 1; numProcs <= 28; numProcs = numProcs + 3) {
				if (numProcs == 1)
					std::cerr << "********************\n";
				std::cerr << results[j][i][numProcs] / (num_iter - 1) / _gThroughputTime << "\n";
			}
#endif
}
else {
	std::cerr << results[j_val][1][atoi(argv[1])] / (num_iter - 1) / _gThroughputTime << "\n";
}

	return 0;

}
//*/

/*MyFCQueue tests
 ////////////////////////////////////////////////////////////////////////////////
 //CONSTS
 ////////////////////////////////////////////////////////////////////////////////

 #include "MyFCQueue.h"//my data structure

 static final int _ACTIONS_ARY_SIZE = 2*1024*1024;

 static int						_num_ds;
 static ITest*					_gDS[1024];
 static Configuration			_gConfiguration;

 static Random					_gRand;
 static int						_gActionAry[_ACTIONS_ARY_SIZE];
 static int						_gTotalRandNum;

 static int						_g_thread_fill_table_size;
 static int*						_gRandNumAry;
 static tick_t volatile *		_gThreadResultAry;

 static int						_gNumProcessors;
 int						_gNumThreads;
 static int						_gThroughputTime;

 static Thread**					_gThreads;
 static AtomicInteger			_gThreadStartCounter(0);
 static AtomicInteger			_gThreadEndCounter(0);
 static VolatileType<tick_t>		_gStartTime(0UL);
 static VolatileType<tick_t>		_gEndTime(0UL);
 static VolatileType<int>		_gIsStopThreads(0UL);

 static tick_t					_gResult = 0L;
 static tick_t					_gResultAdd = 0L;
 static tick_t					_gResultRemove = 0L;
 static tick_t					_gResultPeek = 0L;

 static _u64 volatile			_seed;
 static boolean					_is_tm=false;
 static boolean					_is_view=false;

 static ITest*					_myDS;//
 static int 						_initial_size;//
 static volatile int 			_executionMode;//
 static 	int 					_nanosec;//
 static AtomicInteger 			_totalOP(0);//
 static AtomicInteger			_totalSpecificOP[2];
 //static AtomicInteger 			_initialOP(0);//


 ////////////////////////////////////////////////////////////////////////////////
 //TYPEDEFS
 ////////////////////////////////////////////////////////////////////////////////
 class MixThread : public Thread {
 public:
 final int _threadNo;
 int _totalNum;
 int _trueNum;
 int _falseNum;
 int _evenNum;
 int _oddNum;
  Random _local_rand;


 MixThread (final int inThreadNo) : _threadNo(inThreadNo), _totalNum(0), _trueNum(0), _falseNum(0),
 _evenNum(0), _oddNum(0) {}

 inline_ void threadPause (int nanosec)
 {
 if (nanosec > 0) {
 struct timespec time, time2;
 time.tv_nsec = nanosec;
 time.tv_sec = 0;
 nanosleep(&time, &time2);
 }
 }


 void run() {

 const int arraySize = 500;
 int currVal;
 int volatile dequed_val;
 MyFCQueue::RNode* nodes[arraySize];

 //////////////////////////////////////////
 //initialize real nodes to be enqueued
 //////////////////////////////////////////

 while (_executionMode == -1) {
 //currVal = _local_rand.nextInt(_initial_size) + 1;

 ((MyFCQueue*) _myDS)->add(_threadNo, new MyFCQueue::RNode(100 + _threadNo));
 //_initialOP.getAndIncrement();
 }

 if (_executionMode == 0) { //balanced
 do {

 //threadPause(_nanosec);

 if (_totalNum % 1000 == 0) {
 for (int i = 0; i < arraySize; i++) {
 nodes[i] = new MyFCQueue::RNode(100 + _threadNo);
 }
 }

 //currVal = _local_rand.nextInt(_initial_size) + 1;

 //if (currVal % 2 == 0)////////////////try different distributions
 //	currVal = currVal / 2;///////////////

 if (_totalNum % 2 == 0)
 ((MyFCQueue*) _myDS)->add(_threadNo, nodes[_totalNum % 1000 / 2]);
 else {
 MyFCQueue::RNode* node = ((MyFCQueue*) _myDS)->remove(_threadNo);
 if (node != null) {
 dequed_val = node->_value;
 delete(node);
 }
 }

 ++_totalNum;

 if (0 != _gIsStopThreads) {
 break;
 }

 } while(true);
 }

 if (_executionMode == 1) { //more enqs
 do {

 //threadPause(_nanosec);

 //if (_executionMode == 3)
 //std::cerr << "new testing\n";

 _totalNum++;

 //if (_totalNum % 4 == 0)
 ((MyFCQueue*) _myDS)->add(_threadNo, new MyFCQueue::RNode(100 + _threadNo));
 //else
 //..........

 if (0 != _gIsStopThreads) {
 break;
 }

 } while (true);
 }

 if (_executionMode == 2) { //more deqs
 do {
 //threadPause(_nanosec);

 _totalNum++;

 //if (currVal % 2 == 0)////////////////try different distributions
 //	currVal = currVal / 2;///////////////

 //if (_totalNum % 4 != 0)
 //...........
 //else
 {
 MyFCQueue::RNode* node = ((MyFCQueue*) _myDS)->remove(_threadNo);
 if (node != null)
 dequed_val = node->_value;
 delete(node);
 }

 if (0 != _gIsStopThreads) {
 break;
 }

 } while (true);
 }

 if (_executionMode == 3) { //balanced: half of threads enqueue and half dequeue
 do {

 //threadPause(_nanosec);

 _totalNum++;

 //currVal = _local_rand.nextInt(_initial_size) + 1;

 if (_threadNo % 2 == 0)
 ((MyFCQueue*) _myDS)->add(_threadNo, new MyFCQueue::RNode(100 + _threadNo));
 else {
 MyFCQueue::RNode* node = ((MyFCQueue*) _myDS)->remove(_threadNo);
 if (node != null)
 dequed_val = node->_value;
 delete(node);
 }

 if (0 != _gIsStopThreads) {
 break;
 }

 } while (true);
 }


 sleep(_local_rand.nextInt(2000));
 //std::cerr << "Thread " << _threadNo << " #requests: " << _totalNum
 //		<< ", #true's: " << _trueNum << "/" << _evenNum <<
 //		", #false's: " << _falseNum << "/" << _oddNum << "\n";

 //std::cerr << "Thread " << _threadNo << " #requests: " << _totalNum << "\n";
 //std::cerr << _totalNum << " operations / thread \n";
 if (_executionMode < 3)
 _totalOP.getAndAdd(_totalNum);
 else
 _totalSpecificOP[_threadNo % 2].getAndAdd(_totalNum);

 }
 };

 //void singleFCExecution();
 //void sequetialExecution();
 //void multiFCExecution();

 int main(int argc, char **argv) {

 string cases[4] = {"(balanced)", "(more enqs)", "(more deqs)", "(balanced: dedicated threads)"};


 for (int numProcs = 1; numProcs <= 64; numProcs = numProcs + 3)
 for (int i = 0; i <= 0; i++) //0~3
 for (int k = 4; k <= 4; k++)
 for (int iter = 0; iter < 3; iter++)
 {

 //contaminate memory manager ...............................................
 for (int ii=0; ii< (1024*64); ++ii) {
 void* final rand_mem = malloc( _gRand.nextInt(128)+1 );
 free(rand_mem);
 }

 //initialize global variables ..............................................
 _gNumProcessors = numProcs; //Runtime.getRuntime().availableProcessors();
 _gNumThreads = _gNumProcessors;
 //_gTotalRandNum      = Math::Max(_gConfiguration._capacity, 4*1024*1024);
 _gThroughputTime = 2; //seconds
 _gIsStopThreads = 0;
 _nanosec = 0;
 _totalOP.set(0);

 _totalSpecificOP[0].set(0); //enq
 _totalSpecificOP[1].set(0);	//deq
 //_initialOP.set(0);

 if (k == 0)
 _initial_size = 1000;
 if (k == 1)
 _initial_size = 10000;
 if (k == 2)
 _initial_size = 100000;
 if (k == 3)
 _initial_size = 1000000;
 if (k == 4)
 _initial_size = 7000000;

 _executionMode = -1; //initialization

 string pause;
 std::cout << "start initializing\n";
 //	std::cin >> pause;

 Memory::read_write_barrier();

 std::cerr << "============= #processors and #threads " << _gNumProcessors << " and "
 << _gNumThreads << " " + cases[i] << "\n";

 _myDS = new MyFCQueue();

 //return 0;

 Memory::read_write_barrier();

 //std::cerr << "#nodes before a run: " << ((MyFCLinkedList*) _myDS)->node_count() << " with size " << _list_size << "\n";

 //calculate how much each thread should add the data-structure initially ...
 //int table_size  = (int)((_gConfiguration._capacity) * (_gConfiguration._load_factor));
 //_g_thread_fill_table_size = table_size / _gNumThreads;

 //create benchmark threads .................................................
 //System_err_println("    START creating threads.");
 _gThreads = new Thread*[_gNumThreads];
 //_gThreadResultAry       =  new tick_t[_gNumThreads];
 //memset((void*)_gThreadResultAry, 0, sizeof(int)*_gNumThreads);

 for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
 _gThreads[iThread] = new MixThread(iThread);
 }


 cpu_set_t cpuset;
 int s;

 for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
 _gThreads[iThread] = new MixThread(iThread);

 //node 0: 0, 4, 8...; node 1: 1, 5, 9...; node 2:...
 int coreNo = (iThread % 28) * 4 + (iThread / 28);

 CPU_ZERO(&cpuset);
 CPU_SET(coreNo, &cpuset);

 s = pthread_setaffinity_np(_gThreads[iThread]->_handle, sizeof(cpu_set_t), &cpuset);
 if (s != 0)
 std::cerr << "error: pthread_setaffinity_np\n";

 s = pthread_getaffinity_np(_gThreads[iThread]->_handle, sizeof(cpu_set_t), &cpuset);
 if (s != 0)
 std::cerr << "error 2: pthread_setaffinity_np\n";
 }


 //System_err_println("    END   creating threads.");
 //System_err_println("");
 Thread::yield();

 //start the benchmark threads ..............................................
 //System_err_println("    START threads.");
 for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
 _gThreads[iThread]->start();
 }
 //System_err_println("    END START  threads.");
 //System_err_println("");

 //wait the throughput time, and then signal the threads to terminate ...
 Thread::yield();

 while (((MyFCQueue*) _myDS)->_length < _initial_size) {
 Thread::sleep(1);
 }

 //std::cerr << "queue length: " << ((MyFCQueue*) _myDS)->_length << "\n";


 _executionMode = i;
 Memory::read_write_barrier();
 Thread::yield();
 Thread::sleep(_gThroughputTime * 1000);

 _gIsStopThreads = 1;

 //join the threads .........................................................
 for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
 Thread::yield();
 Thread::yield();
 _gThreads[iThread]->join();
 }


 //std::cerr << "average #operations: " << _totalOP.get() / _gNumThreads << "\n";
 if (_executionMode < 3)
 std::cerr << "***total #operations: " << _totalOP.get() <<
 "; #average combined operation: "<< ((double) _myDS->_FCCombinedCount) / _myDS->_FCCount
 << "; #nodes remained: " << ((MyFCQueue*) _myDS)->_length << "\n";
 else
 std::cerr << "***#enqs: " << _totalSpecificOP[0].get() << "\t #deqs: " << _totalSpecificOP[1].get() << "\n";



 //System_err_println("    ALL threads terminated.");
 System_err_println("");

 for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
 _gThreads[iThread]->destroy();
 delete _gThreads[iThread];
 }

 delete[] _gThreads;
 _gThreads = null;

 //		string pause;
 //		std::cout << "start freeing\n";
 //		std::cin >> pause;

 delete _myDS;

 //std::cerr << "memory freed\n";

 Thread::sleep(1000);

 //int x;
 //std::cin >> x;

 //	std::cin >> pause;

 }

 return 0;
 //end of my simple experiments
 //////////////////////////////////////////////////////////

 }
 //*/

/*queue tests
 ////////////////////////////////////////////////////////////////////////////////
 //CONSTS
 ////////////////////////////////////////////////////////////////////////////////
 static final int _ACTIONS_ARY_SIZE = 2*1024*1024;

 static int						_num_ds;
 static ITest*					_gDS[1024];
 static Configuration			_gConfiguration;

 static Random					_gRand;
 static int						_gActionAry[_ACTIONS_ARY_SIZE];
 static int						_gTotalRandNum;

 static int						_g_thread_fill_table_size;
 static int*						_gRandNumAry;
 static tick_t volatile *		_gThreadResultAry;

 static int						_gNumProcessors;
 int						_gNumThreads;
 static int						_gThroughputTime;

 static Thread**					_gThreads;
 static AtomicInteger			_gThreadStartCounter(0);
 static AtomicInteger			_gThreadEndCounter(0);
 static VolatileType<tick_t>		_gStartTime(0UL);
 static VolatileType<tick_t>		_gEndTime(0UL);
 static VolatileType<int>		_gIsStopThreads(0UL);

 static tick_t					_gResult = 0L;
 static tick_t					_gResultAdd = 0L;
 static tick_t					_gResultRemove = 0L;
 static tick_t					_gResultPeek = 0L;

 static _u64 volatile			_seed;
 static boolean					_is_tm=false;
 static boolean					_is_view=false;

 static ITest*					_myDS;//
 static int 						_initial_size = 1000000;//
 static volatile int 			_executionMode;//
 static 	int 					_nanosec;//
 static AtomicInteger 			_totalOP(0);//
 static AtomicInteger			_totalSpecificOP[2];
 //static AtomicInteger 			_initialOP(0);//


 ////////////////////////////////////////////////////////////////////////////////
 //TYPEDEFS
 ////////////////////////////////////////////////////////////////////////////////
 class MixThread : public Thread {
 public:
 final int _threadNo;
 int _totalNum;
 int _trueNum;
 int _falseNum;
 int _evenNum;
 int _oddNum;
 Random _local_rand;


 MixThread (final int inThreadNo) : _threadNo(inThreadNo), _totalNum(0), _trueNum(0), _falseNum(0),
 _evenNum(0), _oddNum(0) {}

 inline_ void threadPause (int nanosec)
 {
 if (nanosec > 0) {
 struct timespec time, time2;
 time.tv_nsec = nanosec;
 time.tv_sec = 0;
 nanosleep(&time, &time2);
 }
 }


 void run() {

 int currVal = 1;

 while (_executionMode == -1) {
 currVal = _local_rand.nextInt(_initial_size) + 1;
 ((FCQueue*) _myDS)->add(_threadNo, currVal);
 //_initialOP.getAndIncrement();
 }

 if (_executionMode == 0) { //balanced
 do {

 //threadPause(_nanosec);

 _totalNum++;

 //currVal = _local_rand.nextInt(_initial_size) + 1;

 //if (currVal % 2 == 0)////////////////try different distributions
 //	currVal = currVal / 2;///////////////

 if ((_totalNum + currVal) % 2 == 0)
 ((FCQueue*) _myDS)->add(_threadNo, currVal);
 else
 ((FCQueue*) _myDS)->remove(_threadNo, currVal);

 if (0 != _gIsStopThreads) {
 break;
 }

 } while(true);
 }

 if (_executionMode == 1) { //more enqs
 do {

 //threadPause(_nanosec);

 //if (_executionMode == 3)
 //std::cerr << "new testing\n";

 _totalNum++;

 //currVal = _local_rand.nextInt(_initial_size) + 1;

 //if (currVal % 2 == 0)////////////////try different distributions
 //	currVal = currVal / 2;///////////////

 //if (_totalNum % 4 == 0)
 ((FCQueue*) _myDS)->add(_threadNo, currVal);
 //else
 //	((FCQueue*) _myDS)->remove(_threadNo, currVal);

 if (0 != _gIsStopThreads) {
 break;
 }

 } while (true);
 }

 if (_executionMode == 2) { //more deqs
 do {
 //threadPause(_nanosec);

 _totalNum++;

 //currVal = _local_rand.nextInt(_initial_size) + 1;

 //if (currVal % 2 == 0)////////////////try different distributions
 //	currVal = currVal / 2;///////////////

 //if (_totalNum % 4 != 0)
 //	((FCQueue*) _myDS)->add(_threadNo, currVal);
 //else
 ((FCQueue*) _myDS)->remove(_threadNo, currVal);

 if (0 != _gIsStopThreads) {
 break;
 }

 } while (true);
 }

 if (_executionMode == 3) { //balanced: half of threads enqueue and half dequeue
 do {

 //threadPause(_nanosec);

 _totalNum++;

 //currVal = _local_rand.nextInt(_initial_size) + 1;

 if (_threadNo % 2 == 0)
 ((FCQueue*) _myDS)->add(_threadNo, currVal);
 else
 ((FCQueue*) _myDS)->remove(_threadNo, currVal);

 if (0 != _gIsStopThreads) {
 break;
 }

 } while (true);
 }


 sleep(_local_rand.nextInt(2000));
 //std::cerr << "Thread " << _threadNo << " #requests: " << _totalNum
 //		<< ", #true's: " << _trueNum << "/" << _evenNum <<
 //		", #false's: " << _falseNum << "/" << _oddNum << "\n";

 //std::cerr << "Thread " << _threadNo << " #requests: " << _totalNum << "\n";
 if (_executionMode < 3)
 _totalOP.getAndAdd(_totalNum);
 else
 _totalSpecificOP[_threadNo % 2].getAndAdd(_totalNum);

 }
 };

 //void singleFCExecution();
 //void sequetialExecution();
 //void multiFCExecution();

 int main(int argc, char **argv) {

 string cases[4] = {"(balanced)", "(more enqs)", "(more deqs)", "(balanced: dedicated threads)"};


 for (int numProcs = 1; numProcs <= 64; numProcs = numProcs + 3)
 for (int i = 0; i <= 0; i++) //0~3
 for (int k = 4; k <= 4; k++)
 for (int iter = 0; iter < 2; iter++)
 {

 //contaminate memory manager ...............................................
 for (int ii=0; ii< (1024*64); ++ii) {
 void* final rand_mem = malloc( _gRand.nextInt(128)+1 );
 free(rand_mem);
 }

 //initialize global variables ..............................................
 _gNumProcessors = numProcs; //Runtime.getRuntime().availableProcessors();
 _gNumThreads = _gNumProcessors;
 //_gTotalRandNum      = Math::Max(_gConfiguration._capacity, 4*1024*1024);
 _gThroughputTime = 2; //seconds
 _gIsStopThreads = 0;
 _nanosec = 0;
 _totalOP.set(0);

 _totalSpecificOP[0].set(0); //enq
 _totalSpecificOP[1].set(0);	//deq
 //_initialOP.set(0);

 if (k == 0)
 _initial_size = 1000;
 if (k == 1)
 _initial_size = 10000;
 if (k == 2)
 _initial_size = 100000;
 if (k == 3)
 _initial_size = 1000000;
 if (k == 4)
 _initial_size = 7000000;

 _executionMode = -1; //initialization

 string pause;
 std::cout << "start initializing\n";
 //	std::cin >> pause;

 Memory::read_write_barrier();

 std::cerr << "============= #processors and #threads " << _gNumProcessors << " and "
 << _gNumThreads << " " + cases[i] << "\n";

 _myDS = new FCQueue();

 //return 0;

 Memory::read_write_barrier();

 //std::cerr << "#nodes before a run: " << ((MyFCLinkedList*) _myDS)->node_count() << " with size " << _list_size << "\n";

 //calculate how much each thread should add the data-structure initially ...
 //int table_size  = (int)((_gConfiguration._capacity) * (_gConfiguration._load_factor));
 //_g_thread_fill_table_size = table_size / _gNumThreads;

 //create benchmark threads .................................................
 //System_err_println("    START creating threads.");
 _gThreads = new Thread*[_gNumThreads];
 //_gThreadResultAry       =  new tick_t[_gNumThreads];
 //memset((void*)_gThreadResultAry, 0, sizeof(int)*_gNumThreads);

 for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
 _gThreads[iThread] = new MixThread(iThread);
 }

 cpu_set_t cpuset;
 int s;

 for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
 _gThreads[iThread] = new MixThread(iThread);

 CPU_ZERO(&cpuset);
 CPU_SET(iThread, &cpuset);

 s = pthread_setaffinity_np(_gThreads[iThread]->_handle, sizeof(cpu_set_t), &cpuset);
 if (s != 0)
 std::cerr << "error: pthread_setaffinity_np\n";

 s = pthread_getaffinity_np(_gThreads[iThread]->_handle, sizeof(cpu_set_t), &cpuset);
 if (s != 0)
 std::cerr << "error 2: pthread_setaffinity_np\n";
 }

 //System_err_println("    END   creating threads.");
 //System_err_println("");
 Thread::yield();

 //start the benchmark threads ..............................................
 //System_err_println("    START threads.");
 for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
 _gThreads[iThread]->start();
 }
 //System_err_println("    END START  threads.");
 //System_err_println("");

 //wait the throughput time, and then signal the threads to terminate ...
 Thread::yield();

 while (((FCQueue*) _myDS)->_length < _initial_size) {
 Thread::sleep(1);
 }

 //std::cerr << "queue length: " << ((FCQueue*) _myDS)->_length << "\n";


 _executionMode = i;
 Memory::read_write_barrier();
 Thread::yield();
 Thread::sleep(_gThroughputTime  * 1000);

 _gIsStopThreads = 1;

 //join the threads .........................................................
 for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
 Thread::yield();
 Thread::yield();
 _gThreads[iThread]->join();
 }


 //std::cerr << "average #operations: " << _totalOP.get() / _gNumThreads << "\n";
 if (_executionMode < 3)
 std::cerr << "***total #operations: " << _totalOP.get() <<
 "; #average combined operation: "<< ((double)_myDS->_FCCombinedCount) / _myDS->_FCCount << "\n";
 else
 std::cerr << "***#enqs: " << _totalSpecificOP[0].get() << "\t #deqs: " << _totalSpecificOP[1].get() << "\n";



 //System_err_println("    ALL threads terminated.");
 System_err_println("");

 for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
 _gThreads[iThread]->destroy();
 }

 delete[] _gThreads;
 _gThreads = null;

 //		string pause;
 //		std::cout << "start freeing\n";
 //		std::cin >> pause;

 delete _myDS;

 //std::cerr << "memory freed\n";

 Thread::sleep(1000);

 //int x;
 //std::cin >> x;

 //	std::cin >> pause;

 }

 return 0;
 //end of my simple experiments
 //////////////////////////////////////////////////////////

 }
 //*/

/*******************************************************************************
 // * tests for my FC linked list
 // *
 #include "MyFCLinkedList.h"//my data structure
#include "MyFCSkipList.h"//my data structure

 ////////////////////////////////////////////////////////////////////////////////
 //CONSTS
 ////////////////////////////////////////////////////////////////////////////////
 static final int _ACTIONS_ARY_SIZE = 2*1024*1024;

 static int						_num_ds;
 static ITest*					_gDS[1024];
 static Configuration			_gConfiguration;

 static Random					_gRand;
 static int						_gActionAry[_ACTIONS_ARY_SIZE];
 static int						_gTotalRandNum;

 static int						_g_thread_fill_table_size;
 static int*						_gRandNumAry;
 static tick_t volatile *		_gThreadResultAry;

 static int						_gNumProcessors;
 int								_gNumThreads;
 static int						_gThroughputTime;

 static Thread**					_gThreads;
 static AtomicInteger			_gThreadStartCounter(0);
 static AtomicInteger			_gThreadEndCounter(0);
 static VolatileType<tick_t>		_gStartTime(0UL);
 static VolatileType<tick_t>		_gEndTime(0UL);
 static VolatileType<int>		_gIsStopThreads(0UL);

 static tick_t					_gResult = 0L;
 static tick_t					_gResultAdd = 0L;
 static tick_t					_gResultRemove = 0L;
 static tick_t					_gResultPeek = 0L;

 static _u64 volatile			_seed;
 static boolean					_is_tm=false;
 static boolean					_is_view=false;

 static ITest*					_myDS;//
 static ITest**					_myDS_part;//
 static int 					_list_size = 100000;//
 static volatile int 			_executionMode;//
 static volatile int				_num_partitions;
 static 	int 					_nanosec;//
 static AtomicInteger 			_totalOP(0);//
 static AtomicInteger			_specificOP[3];


 ////////////////////////////////////////////////////////////////////////////////
 //TYPEDEFS
 ////////////////////////////////////////////////////////////////////////////////
 class MixThread : public Thread {
 public:
 final int _threadNo;
 int _totalNum;
 int _trueNum;
 int _falseNum;
 int _evenNum;
 int _oddNum;
 Random _local_rand;


 MixThread(final int inThreadNo) :
			_threadNo(inThreadNo), _totalNum(0), _trueNum(0), _falseNum(0), _evenNum(
					0), _oddNum(0) {
	}

	inline_
	void threadPause(int nanosec) {
		if (nanosec > 0) {
			struct timespec time, time2;
			time.tv_nsec = nanosec;
			time.tv_sec = 0;
			nanosleep(&time, &time2);
		}
	}

	void run() {

		int cpuInfo;
		int nodeInfo;

		//threadPause(_threadNo * 1000000);
		//if (getcpu(&cpuInfo, &nodeInfo))
		//	std::cerr << cpuInfo << " " << nodeInfo << " " << sched_getcpu() << "\n";
		//else
		//	std::cerr << "!!!\n";

		int currVal;

		ITest::SlotInfo* volatile local_slot = new ITest::SlotInfo();

		int _part_size = _list_size / _num_partitions;

		ITest::SlotInfo* volatile local_slots[_num_partitions];
		for (int i = 0; i < _num_partitions; i++)
			local_slots[i] = new ITest::SlotInfo();

		if (_executionMode == 0) { //singleFCExecution
			do {

				//threadPause(_nanosec);

				_totalNum++;

				currVal = _local_rand.nextInt(_list_size);

				//if (currVal % 2 == 0)////////////////try different distributions
				//	currVal = currVal / 2;///////////////


				if (_totalNum % 3 == 0) //no contains
					((MyFCLinkedList*) _myDS)->singleFCContain(_threadNo, currVal, local_slot);
				else if (_totalNum % 3 == 1)
					((MyFCLinkedList*) _myDS)->singleFCAdd(_threadNo, currVal, local_slot);
				else
				((MyFCLinkedList*) _myDS)->singleFCDelete(_threadNo, currVal, local_slot);

				if (0 != _gIsStopThreads) {
					break;
				}

			} while (true);
		}

		if (_executionMode == 1) { //parallelExecution
			do {

				//threadPause(_nanosec);

				//if (_executionMode == 3)
				//std::cerr << "new testing\n";

				_totalNum++;

				currVal = _local_rand.nextInt(_list_size);

				//if (currVal % 2 == 0)////////////////try different distributions
				//	currVal = currVal / 2;///////////////

				//if (currVal % 2 == 0 && currVal < _list_size / 2)////////////////try different distributions
				//	currVal = currVal * 2;///////////////

				if (_totalNum % 3 == 0) //no contains
					((MyFCLinkedList*) _myDS)->parallelContain(_threadNo, currVal);
				else if (_totalNum % 3 == 1)
					((MyFCLinkedList*) _myDS)->parallelAdd(_threadNo, currVal);
				else
					((MyFCLinkedList*) _myDS)->parallelDelete(_threadNo, currVal);

				if (0 != _gIsStopThreads) {
					break;
				}

			} while (true);
		}

		if (_executionMode == 2) { //combinedFCExecution
			ITest::SlotInfo* volatile local_slot = new ITest::SlotInfo();

			do {
				//threadPause(_nanosec);

				_totalNum++;

				currVal = _local_rand.nextInt(_list_size);

				if (_totalNum % 3 == 0) //no contains
					((MyFCLinkedList*) _myDS)->combinedFCContain(_threadNo,
							currVal, local_slot);
				else if (_totalNum % 3 == 1)
					((MyFCLinkedList*) _myDS)->combinedFCAdd(_threadNo, currVal,
							local_slot);
				else
					((MyFCLinkedList*) _myDS)->combinedFCDelete(_threadNo,
							currVal, local_slot);

				if (0 != _gIsStopThreads) {
					break;
				}

			} while (true);

		}

		if (_executionMode == 3) { //combinedFC with partitioning
			do {
				//threadPause(_nanosec);
				_totalNum++;
				currVal = _local_rand.nextInt(_list_size);
				int part_id = currVal / _part_size;

				if (_totalNum % 3 == 0) //no contains
					((MyFCLinkedList*) _myDS_part[part_id])->combinedFCContain(
							_threadNo, currVal, local_slots[part_id]);
				else if (_totalNum % 3 == 1)
					((MyFCLinkedList*) _myDS_part[part_id])->combinedFCAdd(
							_threadNo, currVal, local_slots[part_id]);
				else
					((MyFCLinkedList*) _myDS_part[part_id])->combinedFCDelete(
							_threadNo, currVal, local_slots[part_id]);

				if (0 != _gIsStopThreads) {
					break;
				}

			} while (true);

		}

		sleep(_local_rand.nextInt(2000));
		//std::cerr << "Thread " << _threadNo << " #requests: " << _totalNum
		//		<< ", #true's: " << _trueNum << "/" << _evenNum <<
		//		", #false's: " << _falseNum << "/" << _oddNum << "\n";

		//std::cerr << "Thread " << _threadNo << " #requests: " << _totalNum << "\n";
		_totalOP.getAndAdd(_totalNum);
		//_specificOP[_threadNo % 3].getAndAdd(_totalNum);

		delete local_slot;

		for (int i = 0; i < _num_partitions; i++)
			delete local_slots[i];

	}
};



 int main(int argc, char **argv) {

	//contaminate memory manager ...............................................
	for (int i = 0; i < (1024 * 64); ++i) {
		void* final rand_mem = malloc(_gRand.nextInt(128) + 1);
		free(rand_mem);
	}

	int iter_num = 5; // >1
	_gThroughputTime = 5; //5 seconds

	for (int part_num = 2; part_num <= 2; part_num = part_num + 2)
		for (int j = 3; j <= 3; j++) //0-3
			for (int i = 2; i <= 2; i++) //0-3
					{
				std::cerr << "**************************************\n";

				for (int numProcs = 1; numProcs <= 64; numProcs = numProcs + 3) {
					int aver_num = 0;

					for (int iter = 0; iter < iter_num; iter++) {
						//initialize global variables ..............................................
						_gNumProcessors = numProcs; //Runtime.getRuntime().availableProcessors();
						_gNumThreads = _gNumProcessors * 1;
						//_gTotalRandNum      = Math::Max(_gConfiguration._capacity, 4*1024*1024);

						_gIsStopThreads = 0;
						_nanosec = 0;
						_totalOP.set(0);
						_specificOP[0].set(0);
						_specificOP[1].set(0);
						_specificOP[2].set(0);

						if (j == 0)
							_list_size = 100000;
						if (j == 1)
							_list_size = 500000;
						if (j == 2)
							_list_size = 1000000;
						if (j == 3)
							_list_size = 4000000;

						_executionMode = i;

						_num_partitions = part_num;

						int _part_size = _list_size / _num_partitions;

						//std::cout << "============= #processors and #threads " << _gNumProcessors << " and "	<< _gNumThreads << endl;

#if 0
						if (_executionMode == 0)
							std::cerr << "*********Naive FC with ";
						if (_executionMode == 1)
							std::cerr << "*********Parallel with ";
						if (_executionMode == 2)
							std::cerr << "***********FC with combination with ";
						if (_executionMode == 3)
							std::cerr << "***********FC with combination with " << _num_partitions << " partitions and ";
						std::cerr << _gNumThreads << " threads **********\n";
#endif

						if (_executionMode != 3) {
							_myDS = new MyFCLinkedList(_list_size);
							//std::cerr << "#nodes initially: " << ((MyFCLinkedList*) _myDS)->node_count() << " with size " << _list_size << "\n";
						}

						else {
							_myDS_part = new ITest*[_num_partitions];

							for (int ii = 0; ii < _num_partitions; ii++) {
								_myDS_part[ii] = new MyFCLinkedList(
										_part_size * ii,
										_part_size * (ii + 1) - 1);
								//std::cerr << "#nodes initially: " << ((MyFCLinkedList*) _myDS_part[ii])->node_count() << " with size " << _part_size << "\n";
							}
						}

						//calculate how much each thread should add the data-structure initially ...
						//int table_size  = (int)((_gConfiguration._capacity) * (_gConfiguration._load_factor));
						//_g_thread_fill_table_size = table_size / _gNumThreads;

						//create benchmark threads .................................................
						//System_err_println("    START creating threads.");
						_gThreads = new Thread*[_gNumThreads];
						//_gThreadResultAry       =  new tick_t[_gNumThreads];
						//memset((void*)_gThreadResultAry, 0, sizeof(int)*_gNumThreads);

						cpu_set_t cpuset;
						int s;

						for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
							_gThreads[iThread] = new MixThread(iThread);

							//node 0: 0, 4, 8...; node 1: 1, 5, 9...; node 2:...
							int coreNo = (iThread % 28) * 4 + (iThread / 28);

							CPU_ZERO(&cpuset);
							CPU_SET(coreNo, &cpuset);

							s = pthread_setaffinity_np(
									_gThreads[iThread]->_handle,
									sizeof(cpu_set_t), &cpuset);
							if (s != 0)
								std::cerr << "error: pthread_setaffinity_np\n";

							s = pthread_getaffinity_np(
									_gThreads[iThread]->_handle,
									sizeof(cpu_set_t), &cpuset);
							if (s != 0)
								std::cerr
										<< "error 2: pthread_setaffinity_np\n";
						}

						//System_err_println("    END   creating threads.");
						//System_err_println("");
						Thread::yield();

						//start the benchmark threads ..............................................
						//System_err_println("    START threads.");
						for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
							_gThreads[iThread]->start();
						}
						//System_err_println("    END START  threads.");
						//System_err_println("");

						//wait the throughput time, and then signal the threads to terminate ...

						Thread::sleep(_gThroughputTime * 1000);
						_gIsStopThreads = 1;

						//join the threads .........................................................
						for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
							Thread::yield();
							Thread::yield();
							_gThreads[iThread]->join();
						}

						//if (((MyFCLinkedList*) _myDS)->_FCCount > 0)
						//	std::cerr << "#requests per combining: " <<
						//		double (((MyFCLinkedList*) _myDS)->_FCCombinedCount) / ((MyFCLinkedList*) _myDS)->_FCCount <<"\n";

						//std::cerr << "\t #nodes after a run: " << ((MyFCLinkedList*) _myDS)->node_count() << "\n";

						//std::cerr << "average #operations: " << _totalOP.get() / _gNumThreads << "\n";

						//std::cerr << _totalOP.get() << "\n";
						if (iter != 0)
							aver_num = aver_num + _totalOP.get();

						//std::cerr << _totalOP.get() << " \n"; //\t operations

						for (int iThread = 0; iThread < _gNumThreads; ++iThread) {
							delete _gThreads[iThread];
						}

						delete[] _gThreads;
						_gThreads = null;

						if (_executionMode != 3) {
							delete _myDS;
						} else {
							for (int ii = 0; ii < _num_partitions; ii++) {
								delete _myDS_part[ii];
							}
						}

						Thread::sleep(1000);

					}

					std::cerr << aver_num / (iter_num - 1) / _gThroughputTime << " \n"; //\t operations
				}

			}

	return 0;
	//end of my simple experiments
	//////////////////////////////////////////////////////////

}

 //*******************************************************************************/

/*******************************************************************************

 //FC research includes .................................
 #include "../data_structures/FCQueue.h"
 #include "../data_structures/MSQueue.h"
 #include "../data_structures/BasketQueue.h"
 #include "../data_structures/LFStack.h"
 #include "../data_structures/FCStack.h"
 #include "../data_structures/LazySkipList.h"
 #include "../data_structures/OyamaQueue.h"
 #include "../data_structures/OyamaQueueCom.h"
 #include "../data_structures/ComTreeQueue.h"
 #include "../data_structures/EliminationStack.h"
 #include "../data_structures/LFSkipList.h"
 #include "../data_structures/FCPairingHeap.h"
 #include "../data_structures/FCSkipList.h"

 ////////////////////////////////////////////////////////////////////////////////
 //CONSTS
 ////////////////////////////////////////////////////////////////////////////////
 static final int _ACTIONS_ARY_SIZE = 2*1024*1024;

 ////////////////////////////////////////////////////////////////////////////////
 //GLOBALS
 ////////////////////////////////////////////////////////////////////////////////
 static int						_num_ds;
 static ITest*					_gDS[1024];
 static Configuration			_gConfiguration;

 static Random					_gRand;
 static int						_gActionAry[_ACTIONS_ARY_SIZE];
 static int						_gTotalRandNum;

 static int						_g_thread_fill_table_size;
 static int*						_gRandNumAry;
 static tick_t volatile *		_gThreadResultAry;

 static int						_gNumProcessors;
 int						_gNumThreads;
 static int						_gThroughputTime;

 static Thread**					_gThreads;
 static AtomicInteger			_gThreadStartCounter(0);
 static AtomicInteger			_gThreadEndCounter(0);
 static VolatileType<tick_t>		_gStartTime(0UL);
 static VolatileType<tick_t>		_gEndTime(0UL);
 static VolatileType<int>		_gIsStopThreads(0UL);

 static tick_t					_gResult = 0L;
 static tick_t					_gResultAdd = 0L;
 static tick_t					_gResultRemove = 0L;
 static tick_t					_gResultPeek = 0L;

 static _u64 volatile			_seed;
 static boolean					_is_tm=false;
 static boolean					_is_view=false;

 ////////////////////////////////////////////////////////////////////////////////
 //FORWARD DECLARETIONS
 ////////////////////////////////////////////////////////////////////////////////
 void RunBenchmark();
 void PrepareActions();
 void PrepareRandomNumbers(final int size);
 int NearestPowerOfTwo(final int x);
 ITest* CreateDataStructure(char* final alg_name);

 ////////////////////////////////////////////////////////////////////////////////
 //TYPEDEFS
 ////////////////////////////////////////////////////////////////////////////////
 class MixThread : public Thread {
 public:
 final int _threadNo;

 MixThread (final int inThreadNo) : _threadNo(inThreadNo) {}

 void run() {
 //fill table ...........................................................
 for (int iNum=0; iNum < (_g_thread_fill_table_size/16); ++iNum) {

 for (int iDb=0; iDb<_num_ds; ++iDb) {
 for (int i=0; i<16; ++i) {
 _gDS[iDb]->add(_threadNo, Random::getRandom(_seed, _gConfiguration._capacity) + 2);
 }
 }

 }
 for (int iDb=0; iDb<_num_ds; ++iDb) {
 _gDS[iDb]->cas_reset(_threadNo);
 }

 //save start benchmark time ............................................
 final int start_counter = _gThreadStartCounter.getAndIncrement();
 if(start_counter == (_gNumThreads-1))
 _gStartTime = System::currentTimeMillis();
 while((_gNumThreads) != _gThreadStartCounter.get()) {int i=_gThreadStartCounter.get();}

 //start thread benchmark ...............................................
 tick_t action_counter = 0;
 int iNumAdd = start_counter*1024;
 int iNumRemove = start_counter*1024;
 int iNumContain = start_counter*1024;
 int iOp = start_counter*128;
 do {
 final int op = _gActionAry[iOp];

 if(1==op) {
 for (int iDb=0; iDb<_num_ds; ++iDb) {
 final int enq_value = _gDS[iDb]->add(_threadNo, _gRandNumAry[iNumAdd]);
 ++iNumAdd;
 if(iNumAdd >= _gTotalRandNum)
 iNumAdd=0;
 }
 ++action_counter;
 } else if(2==op) {
 for (int iDb=0; iDb<_num_ds; ++iDb) {
 final int deq_value = _gDS[iDb]->remove(_threadNo, _gRandNumAry[iNumRemove]);
 ++iNumRemove;
 if(iNumRemove >= _gTotalRandNum) { iNumRemove=0; }
 }
 ++action_counter;
 } else {
 for (int iDb=0; iDb<_num_ds; ++iDb) {
 _gDS[iDb]->contain(_threadNo, _gRandNumAry[iNumContain]);
 ++iNumContain;
 if(iNumContain >= _gTotalRandNum) {	iNumContain=0; }
 }
 ++action_counter;
 }

 ++iOp;
 if (iOp >= _ACTIONS_ARY_SIZE) {
 iOp=0;
 }

 if (_gConfiguration._read_write_delay > 0) {
 _gDS[0]->post_computation(_threadNo);
 }

 if (0 != _gIsStopThreads) {
 break;
 }

 } while(true);

 //save end benchmark time ..............................................
 final int end_counter = _gThreadEndCounter.getAndIncrement();
 if(end_counter == (_gNumThreads-1)) {
 _gEndTime = System::currentTimeMillis();
 }

 //save thread benchmark result .........................................
 _gThreadResultAry[_threadNo] = action_counter;
 }
 };

 ////////////////////////////////////////////////////////////////////////////////
 //MAIN
 ////////////////////////////////////////////////////////////////////////////////
 int main(int argc, char **argv) {

 //..........................................................................
 _seed = Random::getSeed();

 //contaminate memory manager ...............................................
 for (int i=0; i< (1024*64); ++i) {
 void* final rand_mem = malloc( _gRand.nextInt(128)+1 );
 free(rand_mem);
 }

 //read benchmark configuration .............................................
 if(!_gConfiguration.read(argc, argv)) {
 if(!_gConfiguration.read()) {
 System_out_println("USAGE: <algorithm> <testNum> <numThreads> <numActions> <maxKey> <insertOps> <deleteOps> <unrandom> <loadFactor> <badHash> <initialCount> <_throughput_time>");
 System::exit(-1);
 }
 }

 //initialize global variables ..............................................
 _gNumProcessors     = 1; //Runtime.getRuntime().availableProcessors();
 _gNumThreads        = _gConfiguration._no_of_threads;
 _gTotalRandNum      = Math::Max(_gConfiguration._capacity, 4*1024*1024);
 _gThroughputTime    = _gConfiguration._throughput_time;

 //prepare the random numbers ...............................................
 System_err_println("");
 System_err_println("    START create random numbers.");
 PrepareActions();
 PrepareRandomNumbers(_gTotalRandNum);
 System_err_println("    END   creating random numbers.");
 System_err_println("");
 //System.gc();

 //run the benchmark ........................................................
 RunBenchmark();

 //print results ............................................................
 if(0 == _gConfiguration._is_dedicated_mode) {
 System_out_format(" %d", (unsigned int)_gResult);
 } else {
 System_out_format(" %d %d %d", (unsigned int)_gResultAdd, (unsigned int)_gResultRemove, (unsigned int)_gResultPeek);
 }
 Thread::sleep(1*1000);
 for (int iDb=0; iDb<_num_ds; ++iDb) {
 _gDS[iDb]->print_custom();
 delete _gDS[iDb];
 _gDS[iDb] = null;
 }
 }_gNumThreads

 ////////////////////////////////////////////////////////////////////////////////
 //HELPER FUNCTIONS
 ////////////////////////////////////////////////////////////////////////////////
 void RunBenchmark() {
 //print test information ...................................................
 System_err_println("Benchmark Curr: ");
 System_err_println("--------------");
 System_err_println("    numOfThreads:      " + Integer::toString( _gConfiguration._no_of_threads));

 System_err_println("    Algorithm1 Name:   " + std::string(_gConfiguration._alg1_name));
 System_err_println("    Algorithm1 Num:    " + Integer::toString(_gConfiguration._alg1_num));
 System_err_println("    Algorithm2 Name:   " + std::string(_gConfiguration._alg2_name));
 System_err_println("    Algorithm2 Num:    " + Integer::toString(_gConfiguration._alg2_num));
 System_err_println("    Algorithm3 Name:   " + std::string(_gConfiguration._alg3_name));
 System_err_println("    Algorithm3 Num:    " + Integer::toString(_gConfiguration._alg3_num));
 System_err_println("    Algorithm4 Name:   " + std::string(_gConfiguration._alg4_name));
 System_err_println("    Algorithm4 Num:    " + Integer::toString(_gConfiguration._alg4_num));

 System_err_println("    NumProcessors:     " + Integer::toString(_gNumProcessors));
 System_err_println("    testNo:            " + Integer::toString(_gConfiguration._test_no));

 System_err_println("    addOps:            " + Integer::toString(_gConfiguration._add_ops));
 System_err_println("    removeOps:         " + Integer::toString(_gConfiguration._remove_ops));
 System_err_println("    throughput_time:   " + Integer::toString(_gConfiguration._throughput_time));
 System_err_println("    is_dedicated_mode: " + Integer::toString(_gConfiguration._is_dedicated_mode));
 System_err_println("    tm_status:         " + Integer::toString(_gConfiguration._tm_status) + (std::string)("   (0=Norm; else View)"));
 System_err_println("    read_write_delay:  " + Integer::toString(_gConfiguration._is_dedicated_mode));

 _is_view = (0 != _gConfiguration._tm_status);

 char _sprintf_str[1024];
 sprintf(_sprintf_str, "%f",  _gConfiguration._load_factor);
 System_err_println("    loadFactor:        " + (std::string)(_sprintf_str));

 System_err_println("    initialCapacity:   " + Integer::toString(_gConfiguration._capacity));
 System_err_println("");
 ITest::_num_post_read_write = _gConfiguration._read_write_delay;

 //create appropriate data-structure ........................................
 _num_ds=0;
 for (int i=0; i<(_gConfiguration._alg1_num); ++i) {
 ITest* tmp = CreateDataStructure(_gConfiguration._alg1_name);
 if(null != tmp) {
 _gDS[_num_ds++] = tmp;
 }
 }
 for (int i=0; i<(_gConfiguration._alg2_num); ++i) {
 ITest* tmp = CreateDataStructure(_gConfiguration._alg2_name);
 if(null != tmp) {
 _gDS[_num_ds++] = tmp;
 }
 }
 for (int i=0; i<(_gConfiguration._alg3_num); ++i) {
 ITest* tmp = CreateDataStructure(_gConfiguration._alg3_name);
 if(null != tmp) {
 _gDS[_num_ds++] = tmp;
 }
 }
 for (int i=0; i<(_gConfiguration._alg4_num); ++i) {
 ITest* tmp = CreateDataStructure(_gConfiguration._alg4_name);
 if(null != tmp) {
 _gDS[_num_ds++] = tmp;
 }
 }

 //calculate how much each thread should add the data-structure initially ...
 final int table_size  = (int)((_gConfiguration._capacity) * (_gConfiguration._load_factor));
 _g_thread_fill_table_size = table_size / _gNumThreads;

 //create benchmark threads .................................................
 System_err_println("    START creating threads.");
 _gThreads               =  new Thread*[_gNumThreads];
 _gThreadResultAry       =  new tick_t[_gNumThreads];
 memset((void*)_gThreadResultAry, 0, sizeof(int)*_gNumThreads);

 final int num_add_threads       = (int) Math::ceil(_gNumThreads * (_gConfiguration._add_ops)/100.0);
 final int num_remove_threads    = (int) Math::floor(_gNumThreads * (_gConfiguration._remove_ops)/100.0);
 final int num_peek_threads      = _gNumThreads - num_add_threads - num_remove_threads;

 if(0 == _gConfiguration._is_dedicated_mode) {
 for(int iThread = 0; iThread < _gNumThreads; ++iThread) {
 _gThreads[iThread] =  new MixThread(iThread);
 }
 } else {
 System_err_println("    num_add_threads:    " + Integer::toString(num_add_threads));
 System_err_println("    num_remove_threads: " + Integer::toString(num_remove_threads));
 System_err_println("    num_peek_threads:   " + Integer::toString(num_peek_threads));

 int curr_thread=0;
 for(int iThread = 0; iThread < num_add_threads; ++iThread) {
 _gThreads[curr_thread] =  new MixThread(curr_thread);
 ++curr_thread;
 }
 for(int iThread = 0; iThread < num_remove_threads; ++iThread) {
 _gThreads[curr_thread] =  new MixThread(curr_thread);
 ++curr_thread;
 }
 for(int iThread = 0; iThread < num_peek_threads; ++iThread) {
 _gThreads[curr_thread] =  new MixThread(curr_thread);
 ++curr_thread;
 }
 }
 System_err_println("    END   creating threads.");
 System_err_println("");
 Thread::yield();

 //start the benchmark threads ..............................................
 System_err_println("    START threads.");
 for(int iThread = 0; iThread < _gNumThreads; ++iThread) {
 _gThreads[iThread]->start();
 }
 System_err_println("    END START  threads.");
 System_err_println("");

 //wait the throughput time, and then signal the threads to terminate ...
 Thread::yield();
 Thread::sleep(_gThroughputTime*1000);
 _gIsStopThreads = 1;

 //join the threads .........................................................
 for(int iThread = 0; iThread < _gNumThreads; ++iThread) {
 Thread::yield();
 Thread::yield();
 _gThreads[iThread]->join();
 }
 System_err_println("    ALL threads terminated.");
 System_err_println("");

 //calculate threads results ................................................
 _gResult = 0;
 _gResultAdd = 0;
 _gResultRemove = 0;
 _gResultPeek = 0;
 if(0 == _gConfiguration._is_dedicated_mode) {
 for(int iThread = 0; iThread < _gNumThreads; ++iThread) {
 _gResult += _gThreadResultAry[iThread];
 }
 } else {
 int curr_thread=0;
 for(int iThread = 0; iThread < num_add_threads; ++iThread) {
 _gResultAdd += _gThreadResultAry[curr_thread];
 ++curr_thread;
 }
 for(int iThread = 0; iThread < num_remove_threads; ++iThread) {
 _gResultRemove += _gThreadResultAry[curr_thread];
 ++curr_thread;
 }
 for(int iThread = 0; iThread < num_peek_threads; ++iThread) {
 _gResultPeek += _gThreadResultAry[curr_thread];
 ++curr_thread;
 }
 }

 //print benchmark results ..................................................
 for (int iDb=0; iDb<_num_ds; ++iDb) {
 System_err_println("    " + std::string(_gDS[iDb]->name()) + " Num elm: " + Integer::toString(_gDS[iDb]->size()));
 }

 //free resources ...........................................................
 delete [] _gRandNumAry;
 delete [] _gThreadResultAry;
 delete [] _gThreads;

 _gRandNumAry = null;
 _gThreadResultAry = null;
 _gThreads = null;

 //return benchmark results ................................................
 _gResult         /= (long)(_gEndTime - _gStartTime);
 _gResultAdd      /= (long)(_gEndTime - _gStartTime);
 _gResultRemove   /= (long)(_gEndTime - _gStartTime);
 _gResultPeek     /= (long)(_gEndTime - _gStartTime);
 }

 ITest* CreateDataStructure(char* final alg_name) {
 //queue ....................................................................
 if(0 == strcmp(alg_name, "fcqueue")) {
 return (new FCQueue());
 }
 if(0 == strcmp(alg_name, "msqueue")) {
 return (new MSQueue());
 }
 if(0 == strcmp(alg_name, "basketqueue")) {
 return (new BasketsQueue());
 }
 if(0 == strcmp(alg_name, "oyqueue")) {
 return (new OyamaQueue());
 }
 if(0 == strcmp(alg_name, "oyqueuecom")) {
 return (new OyamaQueueCom());
 }
 if(0 == strcmp(alg_name, "ctqueue")) {
 return (new ComTreeQueue());
 }

 //stack ....................................................................
 if(0 == strcmp(alg_name, "lfstack")) {
 return (new LFStack());
 }
 if(0 == strcmp(alg_name, "fcstack")) {
 return (new FCStack());
 }
 if(0 == strcmp(alg_name, "elstack")) {
 return (new EliminationStack());
 }

 //skiplist .................................................................
 if(0 == strcmp(alg_name, "lfskiplist")) {
 return (new LFSkipList());
 }
 if(0 == strcmp(alg_name, "fcpairheap")) {
 return (new FCPairHeap());
 }
 if(0 == strcmp(alg_name, "fcskiplist")) {
 return (new FCSkipList());
 }
 if(0 == strcmp(alg_name, "lazyskiplist")) {
 return (new LazySkipList());
 }

 //..........................................................................
 return null;
 }


 void PrepareActions() {
 final int add_limit = _gConfiguration._add_ops;
 final int remove_limit = add_limit + _gConfiguration._remove_ops;

 for(int iAction=0; iAction < _ACTIONS_ARY_SIZE; ++iAction) {
 final int rand_num = _gRand.nextInt(1024*1024)%100;

 if(rand_num < 0 || rand_num >= 100) {
 System_err_println("PrepareActions: Error random number" + Integer::toString(rand_num));
 System::exit(1);
 }

 if(rand_num < add_limit)
 _gActionAry[iAction] = 1;
 else if(rand_num < remove_limit)
 _gActionAry[iAction] = 2;
 else
 _gActionAry[iAction] = 3;
 }
 }
 void PrepareRandomNumbers(final int size) {
 _gRandNumAry = new int[size];
 for (int iRandNum = 0; iRandNum < size; ++iRandNum) {
 if(0 == _gConfiguration._capacity) {
 _gRandNumAry[iRandNum] = iRandNum+2;
 } else {
 _gRandNumAry[iRandNum] = _gRand.nextInt(_gConfiguration._capacity) + 2;
 if(_gRandNumAry[iRandNum] <= 0 || _gRandNumAry[iRandNum] >= (_gConfiguration._capacity+2)) {
 System_err_println("PrepareRandomNumbers: Error random number" + Integer::toString(_gRandNumAry[iRandNum]));
 System::exit(1);
 }
 }
 }
 }

 int NearestPowerOfTwo(final int x) {
 int mask = 1;
 while(mask < x) {
 mask <<= 1;
 }
 return mask;
 }

 *******************************************************************************/
