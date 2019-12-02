#include <iostream>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <string>
#include <string.h>
#include <vector>
#include <sstream>
#include <math.h>
using namespace std;

int hit;                                        //this variable will be used for calculating no of hits 
int pagefault;					//this variable will be used for calculating no of pagefault 
int sizeOfMainMemory;				//max size of main memory
int segmentLength=1;				//length of segment(The segment length is considered as 1 if not given	

//we can edit the given input file and enter segmentLength=1 in the given sequence of input if not possible then also we can fix its value as 1 here 			
int pageSize;					//size of page				
int pageFramePerProcess;			//no of page frame per process
int lookAheadwindow;		
int windowMin;	                                //min pool size		
int windowMax;					//max pool size
int totalProcesses;

typedef struct Address
{
	int segment;
	int page;
	int displacement;
}Address;

typedef struct Frame
{
	bool isAllocated;
	int processID;
	int pageID;
	int segmentID;
	int frameID;
	Frame(int id)
	{
		frameID = id;
	}
}Frame;

typedef struct Page
{
	bool isAllocated;
	int frameID;
	int pageID;
	Page(int id)
	{
		pageID = id;
		frameID = 0;
		isAllocated = false;
	}
}Page;

typedef struct PageTable
{
	vector<Page> pages;
}PageTable;

typedef struct Segment
{
	int segmentID;
	PageTable pageTable;
	Segment(int id)
	{
		segmentID = id;
	}
}Segment;

typedef struct SegmentTable
{
	vector<Segment> segments;
}SegmentTable;

typedef struct AddressSpace
{
	vector<SegmentTable> segmentTables;		
	vector<Page> LastPagesUsed;
}AddressSpace;

typedef struct Process						
{
	int processID;						
	int totalPageFramesOnDisk;
	AddressSpace addressSpace;	//Structure of structures				 
}Process;
Process *processes;							

vector<Frame> framesInMainMemory;			
vector<Frame> framesInDiskTable;			
vector<string> instructions;
				
void ClearForNextPageReplacement()
{
	framesInMainMemory.clear();
	framesInDiskTable.clear();
	delete[] processes;
	processes = NULL;
}

//construct main memory but putting the frames into it
void BuildMainMemory(int maxFrameSize)
{
	for (int i = 0; i < maxFrameSize; i++)
	{
		framesInMainMemory.push_back(*(new Frame(i)));
	}
}

//Putting the selected process inside the disk
void AllocateIntoDisk(Process *pProcess)			
{
	if (pProcess->addressSpace.segmentTables.size() == 0)
	{
		pProcess->addressSpace.segmentTables.push_back(*(new SegmentTable()));
	}
	for (int i = 0; i < pProcess->totalPageFramesOnDisk; i++)
	{
		int page = i % segmentLength;
		int segment = i / segmentLength;
		if (segment >= pProcess->addressSpace.segmentTables[0].segments.size())
		{
			pProcess->addressSpace.segmentTables[0].segments.push_back(*(new Segment(segment)));
		}
		if (page >= pProcess->addressSpace.segmentTables[0].segments[segment].pageTable.pages.size())
		{
			pProcess->addressSpace.segmentTables[0].segments[segment].pageTable.pages.push_back(*(new Page(page)));
		}

		Frame *pFrame = new Frame(i + framesInDiskTable.size());
		pFrame->isAllocated = true;
		pFrame->pageID = page;
		pFrame->segmentID = segment;
		pFrame->processID = pProcess->processID;
		
		framesInDiskTable.push_back(*pFrame);
	}
}

//First In First Out Algorithm

void FIFO_PageReplacement()
{ 
	BuildMainMemory(sizeOfMainMemory);
	processes = new Process[totalProcesses];

	for (size_t i = 0; i < totalProcesses; i++)
	{
		istringstream iss(instructions[i]);
		string str;
		iss >> str;
		processes[i].processID = stoi(str);
		iss >> str;
		processes[i].totalPageFramesOnDisk = stoi(str);
		AllocateIntoDisk(&processes[i]);
	}
	
	for (size_t j = totalProcesses; j < instructions.size(); j++)
	{
		string instruction = instructions[j];
		
		for (size_t i = 0; i < totalProcesses; i++)
		{
			string processIDtoString = to_string(processes[i].processID);
			if (instruction.find(processIDtoString) != string::npos)
			{
				string addressString = instruction.erase(0, processIDtoString.length() + 1);

				char *cstr = new char[addressString.length() + 1];
				strcpy(cstr, addressString.c_str());
				char *pLine;
				unsigned int addressToInt = (unsigned int)strtol(cstr, &pLine, 0);
				if (((int)addressToInt) < 0) 
				{
					continue;
				}
		
				int displacement = addressToInt & ((1 << (int)log2(pageSize)) - 1);
				int pageID = (addressToInt >> (int)log2(pageSize)) & ((1 << (int)log2(segmentLength)) - 1);
				int segmentID = addressToInt >> ((int)log2(pageSize) + (int)log2(segmentLength));

				cout<<"Attempting page access with Process, Segment, Page, Segment Length \n"<< i, segmentID, pageID, processes[i].addressSpace.segmentTables[0].segments.size();
				
				Page *pPageAccessing = &processes[i].addressSpace.segmentTables[0].segments[segmentID].pageTable.pages[pageID];

				Frame *pFrame = NULL;
				bool PageFault = false;
			
				if (pPageAccessing->isAllocated == false)
				{
					PageFault = true;
					pagefault++;
					cout << "\033[0;31m * Page fault \033[0m" << endl;
				}
				
				else if (!(framesInMainMemory[pPageAccessing->frameID].processID == processes[i].processID &&
					framesInMainMemory[pPageAccessing->frameID].pageID == pageID &&
					framesInMainMemory[pPageAccessing->frameID].segmentID == segmentID))
				{
					PageFault = true;
					cout << "\033[0;33m * Page fault (Was Replaced) \033[0m" << endl;
					hit++;
				}

				if (PageFault)
				{
					
					if (processes[i].addressSpace.LastPagesUsed.size() < pageFramePerProcess)
					{
						
						for (int k = 0; k < framesInMainMemory.size(); k++)
						{
							if (framesInMainMemory[k].isAllocated == false)
							{
								pFrame = &framesInMainMemory[k];
								cout << "\033[0;32m Found unallocated frame in main memory. \033[0m" << endl;
								break;
							}
						}
						if (pFrame == NULL)
						{
							cout<<"\033[0;31m *ERROR*, pFrame==NULL \033[0m\n";
							break;
						}
					}
					else //Replace Page
					{
						pFrame = &framesInMainMemory[processes[i].addressSpace.LastPagesUsed[0].frameID];
						processes[i].addressSpace.LastPagesUsed.pop_back();
						cout<<"\033[0;32m Replaced frame %d...\033[0m\n"<<pFrame->frameID;
					}
					//cout<<"ALlocating to Frame(%d)\n", pFrame->frameID);
					pFrame->isAllocated = true;

					pPageAccessing->frameID = pFrame->frameID;

					pFrame->processID = processes[i].processID;
					pFrame->pageID = pageID;
					pFrame->segmentID = segmentID;
				
					pPageAccessing->isAllocated = true;
					processes[i].addressSpace.LastPagesUsed.push_back(*pPageAccessing);
				}
				else
				{
					hit++;
					cout<<"\033[0;32m Page Found. \033[0m\n";
				}

				break;
			}
		}
	}
}
	

//LRU alogorithm
void LRU_PageReplacement()
{
	vector<Page*> LRU_Array;
	 
	BuildMainMemory(sizeOfMainMemory);

	processes = new Process[totalProcesses];

	for (size_t i = 0; i < totalProcesses; i++)
	{
		istringstream iss(instructions[i]);
		string str;
		processes[i].processID = stoi(str);

		iss >> str;
		processes[i].totalPageFramesOnDisk = stoi(str);
		AllocateIntoDisk(&processes[i]);
	}

	for (size_t j = totalProcesses; j < instructions.size(); j++)
	{
		string instruction = instructions[j];

		for (size_t i = 0; i < totalProcesses; i++)
		{
			string processIDtoString = to_string(processes[i].processID);
			if (instruction.find(processIDtoString) != string::npos)
			{
				string addressString = instruction.erase(0, processIDtoString.length() + 1);
				char *cstr = new char[addressString.length() + 1];
				strcpy(cstr, addressString.c_str());
				char *pLine;
				unsigned int addressToInt = (unsigned int)strtol(cstr, &pLine, 0);

				if (((int)addressToInt) < 0)
					continue;

				int displacement = addressToInt & ((1 << (int)log2(pageSize)) - 1);
				int pageID = (addressToInt >> (int)log2(pageSize)) & ((1 << (int)log2(segmentLength)) - 1);
				int segmentID = addressToInt >> ((int)log2(pageSize) + (int)log2(segmentLength));

				cout<<"Attempting page access with Process, Segment, Page , Segment Length .\n"<< i, segmentID, pageID, processes[i].addressSpace.segmentTables[0].segments.size();
				Page *pPageAccessing = &processes[i].addressSpace.segmentTables[0].segments[segmentID].pageTable.pages[pageID];

				Frame *pFrame = NULL;
				bool PageFault = false;
				
				if (pPageAccessing->isAllocated == false)
				{
					PageFault = true;

					cout << "\033[0;31m * Page fault \033[0m" << endl;
				}
				
				else if (!(framesInMainMemory[pPageAccessing->frameID].processID == processes[i].processID &&
					framesInMainMemory[pPageAccessing->frameID].pageID == pageID &&
					framesInMainMemory[pPageAccessing->frameID].segmentID == segmentID))
				{
					PageFault = true;
					cout << "\033[0;33m * Page fault (Was Replaced) \033[0m" << endl;
				}

				if (PageFault)
				{
						pagefault++;
					if (LRU_Array.size() < pageFramePerProcess)
					{
						
						for (int k = 0; k < framesInMainMemory.size(); k++)
						{
							if (framesInMainMemory[k].isAllocated == false)
							{
								pFrame = &framesInMainMemory[k];
								cout << "\033[0;32m Found unallocated frame in main memory. \033[0m" << endl;
								
								break;
							}
						}
						if (pFrame == NULL)
						{
							cout<<"\033[0;31m *ERROR*, pFrame==NULL \033[0m\n";
							break;
						}
					}
					else //Replace Page
					{
						
						pFrame = &framesInMainMemory[LRU_Array[0]->frameID];

						LRU_Array.erase(LRU_Array.begin());

						cout<<"\033[0;32m Replaced frame %d...\033[0m\n", pFrame->frameID;
					}

					pFrame->isAllocated = true;
					pFrame->processID = processes[i].processID;
					pFrame->pageID = pageID;
					pFrame->segmentID = segmentID;
					
					//	set page to know what frame
					pPageAccessing->frameID = pFrame->frameID;
					pPageAccessing->isAllocated = true;

					i--;
					continue;
					}
				else
				{
					hit++;
					//	Search if the page already exist, remove it from head
					for (size_t l = 0; l < LRU_Array.size(); l++)
						if (LRU_Array[l]->pageID == pPageAccessing->pageID)
						{
							LRU_Array.erase(LRU_Array.begin());
							break;
						}

					//	Push back the new page pointer
					LRU_Array.push_back(pPageAccessing);
					cout<<"\033[0;32m Page Found. \033[0m\n";
				}

				break;
			}
		}
	}
}

//OPT ALGORITHM----there can be an error in giving the correct no of page faults(sometimes)
void OPT_PageReplacement()
{
	cout << "Delta: " << sizeOfMainMemory << endl << endl;

	vector<Page*> WS_Array;

	BuildMainMemory(sizeOfMainMemory);

	processes = new Process[totalProcesses];

	for (size_t i = 0; i < totalProcesses; i++)
	{
		istringstream iss(instructions[i]);

		string str;
		iss >> str;
		processes[i].processID = stoi(str);

		iss >> str;
		processes[i].totalPageFramesOnDisk = stoi(str);
		AllocateIntoDisk(&processes[i]);
	}

	for (size_t j = totalProcesses; j < instructions.size(); j++)
	{
		string instruction = instructions[j];

		for (size_t i = 0; i < totalProcesses; i++)
		{
			string processIDtoString = to_string(processes[i].processID);
			if (instruction.find(processIDtoString) != string::npos)
			{
				
				string addressString = instruction.erase(0, processIDtoString.length() + 1);

				char *cstr = new char[addressString.length() + 1];
				strcpy(cstr, addressString.c_str());
				char *pLine;
				unsigned int addressToInt = (unsigned int)strtol(cstr, &pLine, 0);

				if (((int)addressToInt) < 0)
					continue;

				int displacement = addressToInt & ((1 << (int)log2(pageSize)) - 1);
				int pageID = (addressToInt >> (int)log2(pageSize)) & ((1 << (int)log2(segmentLength)) - 1);
				int segmentID = addressToInt >> ((int)log2(pageSize) + (int)log2(segmentLength));
			cout<<"Attempting page access with Process , Segment , Page , Segment Length .\n"<< i, segmentID, pageID, 					processes[i].addressSpace.segmentTables[0].segments.size();
				Page *pPageAccessing = &processes[i].addressSpace.segmentTables[0].segments[segmentID].pageTable.pages[pageID];

				Frame *pFrame = NULL;
				bool PageFault = false;
				//	if the accessing page is allocated, page fault
				if (pPageAccessing->isAllocated == false)
				{
					PageFault = true;

					cout << "\033[0;31m * Page fault \033[0m" << endl;
				}
				//	if the page accessed matches, replace.
				else if (!(framesInMainMemory[pPageAccessing->frameID].processID == processes[i].processID &&
					framesInMainMemory[pPageAccessing->frameID].pageID == pageID &&
					framesInMainMemory[pPageAccessing->frameID].segmentID == segmentID))
				{
					PageFault = true;
					cout << "\033[0;33m * Page fault (Was Replaced) \033[0m" << endl;
				}

				if (PageFault)
				{
					pagefault++;
					if (WS_Array.size() < pageFramePerProcess)
					{
						//	look for empty space in the main memory
						for (int k = 0; k < framesInMainMemory.size(); k++)
					{
					if (framesInMainMemory[k].isAllocated == false)
							{
								pFrame = &framesInMainMemory[k];
								cout << "\033[0;32m Found unallocated frame in main memory. \033[0m" << endl;

								//	Push back the new page pointer
								WS_Array.push_back(pPageAccessing);

								break;
							}
						}
						if (pFrame == NULL)
						{
							cout<<"\033[0;31m *ERROR*, pFrame==NULL \033[0m\n";
							break;
						}
					}
					else //Replace Page
					{
						//	set frame pointer to head
						pFrame = &framesInMainMemory[WS_Array[0]->frameID];

						//	Push back the new page pointer
						WS_Array.push_back(pPageAccessing);

						//	remove head from working set
						WS_Array.erase(WS_Array.begin());

						cout<<"\033[0;32m Replaced frame %d...\033[0m\n", pFrame->frameID;
					}

					//cout<<"ALlocating to Frame(%d)\n", pFrame->frameID);
					pFrame->isAllocated = true;
					pFrame->processID = processes[i].processID;
					pFrame->pageID = pageID;
					pFrame->segmentID = segmentID;

					//	set page to know what frame
					pPageAccessing->frameID = pFrame->frameID;
					pPageAccessing->isAllocated = true;

					i--;
					continue;
				}
				else
				{
					hit++;
					if (WS_Array.size() > pageFramePerProcess)
					{
						//	remove head from working set
						WS_Array.erase(WS_Array.begin());
					}

					cout<<"\033[0;32m Page Found. \033[0m\n";
				}
				break;
			}
		}

		cout << "Working Set: [";
		for (size_t ws = 0; ws < WS_Array.size(); ws++)
		{
			if (ws + 1 == WS_Array.size())
				cout << WS_Array[ws]->frameID;
			else
				cout << WS_Array[ws]->frameID << ", ";
		}
		cout << "]" << endl;
	}
}
				
void WS_PageReplacement()
{
	cout << "Delta: " << sizeOfMainMemory << endl << endl;
	
	//	vector that keeps track of the pages in chronological order from oldest begin to newest end
	vector<Page*> WS_Array;

	//	Initialize the main memory 
	BuildMainMemory(sizeOfMainMemory);

	//	Initalize processes array
	processes = new Process[totalProcesses];

	for (size_t i = 0; i < totalProcesses; i++)
	{
		istringstream iss(instructions[i]);

		//	Cache processID
		string str;
		iss >> str;
		processes[i].processID = stoi(str);

		//	Cache process totalPageFramesOnDisk
		iss >> str;
		processes[i].totalPageFramesOnDisk = stoi(str);
		AllocateIntoDisk(&processes[i]);
	}

	//	Loop through each instruction and perform parse
	for (size_t j = totalProcesses; j < instructions.size(); j++)
	{
		string instruction = instructions[j];

		for (size_t i = 0; i < totalProcesses; i++)
		{
			string processIDtoString = to_string(processes[i].processID);
			if (instruction.find(processIDtoString) != string::npos)
			{
				//	Shave currentLine so it only contains the address string
				string addressString = instruction.erase(0, processIDtoString.length() + 1);

				//	Convert address string to an int
				char *cstr = new char[addressString.length() + 1];
				strcpy(cstr, addressString.c_str());
				char *pLine;
				unsigned int addressToInt = (unsigned int)strtol(cstr, &pLine, 0);

				if (((int)addressToInt) < 0)
					continue;

				//	Parse address for our Displacement, Page, and Segment
				int displacement = addressToInt & ((1 << (int)log2(pageSize)) - 1);
				int pageID = (addressToInt >> (int)log2(pageSize)) & ((1 << (int)log2(segmentLength)) - 1);
				int segmentID = addressToInt >> ((int)log2(pageSize) + (int)log2(segmentLength));

				//	Attempt to page access
				cout<<"Attempting page access with Process , Segment , Page , Segment Length .\n"<< i, segmentID, pageID, processes[i].addressSpace.segmentTables[0].segments.size();
			Page *pPageAccessing = &processes[i].addressSpace.segmentTables[0].segments[segmentID].pageTable.pages[pageID];
			Frame *pFrame = NULL;
				bool PageFault = false;
				//	if the accessing page is allocated, page fault
				if (pPageAccessing->isAllocated == false)
				{
					PageFault = true;

					cout << "\033[0;31m * Page fault \033[0m" << endl;
				}
				//	if the page accessed matches, replace.
				else if (!(framesInMainMemory[pPageAccessing->frameID].processID == processes[i].processID &&
					framesInMainMemory[pPageAccessing->frameID].pageID == pageID &&
					framesInMainMemory[pPageAccessing->frameID].segmentID == segmentID))
				{
					PageFault = true;
					cout << "\033[0;33m * Page fault (Was Replaced) \033[0m" << endl;
				}

				if (PageFault)
				{
					pagefault++;
					if (WS_Array.size() < pageFramePerProcess)
					{
						//	look for empty space in the main memory
						for (int k = 0; k < framesInMainMemory.size(); k++)
						{
							if (framesInMainMemory[k].isAllocated == false)
							{
								pFrame = &framesInMainMemory[k];
								cout << "\033[0;32m Found unallocated frame in main memory. \033[0m" << endl;
						WS_Array.push_back(pPageAccessing);

								break;
							}
						}
						if (pFrame == NULL)
						{
							cout<<"\033[0;31m *ERROR*, pFrame==NULL \033[0m\n";
							break;
						}
					}
					else //Replace Page
					{
						//	set frame pointer to head
						pFrame = &framesInMainMemory[WS_Array[0]->frameID];

						//	Push back the new page pointer
						WS_Array.push_back(pPageAccessing);

						//	remove head from working set
						WS_Array.erase(WS_Array.begin());

						cout<<"\033[0;32m Replaced frame %d...\033[0m\n"<<pFrame->frameID;
					}

					//cout<<"ALlocating to Frame(%d)\n", pFrame->frameID);
					pFrame->isAllocated = true;
					pFrame->processID = processes[i].processID;
					pFrame->pageID = pageID;
					pFrame->segmentID = segmentID;

					//	set page to know what frame
					pPageAccessing->frameID = pFrame->frameID;
					pPageAccessing->isAllocated = true;

					i--;
					continue;
				}
				else
				{
					hit++;
					if (WS_Array.size() > pageFramePerProcess)
					{
						//	remove head from working set
						WS_Array.erase(WS_Array.begin());
					}

					cout<<"\033[0;32m Page Found. \033[0m\n";
				}
				break;
			}
		}

		cout << "Working Set: [";
		for (size_t ws = 0; ws < WS_Array.size(); ws++)
		{
			if (ws+1 == WS_Array.size())
				cout << WS_Array[ws]->frameID;
			else
				cout << WS_Array[ws]->frameID << ", ";
		}
		cout << "]" << endl;
	}
}




void LFU_PageReplacement()
{
	//	vector that keeps track of previous pages
	vector<Page*> History_Array;
	vector<Page*> LFU_Array;
	
	//	Initialize the main memory 
	BuildMainMemory(sizeOfMainMemory);

	//	Initalize processes array
	processes = new Process[totalProcesses];

	for (size_t i = 0; i < totalProcesses; i++)
	{
		istringstream iss(instructions[i]);

		//	Cache processID
		string str;
		iss >> str;
		processes[i].processID = stoi(str);

		//	Cache process totalPageFramesOnDisk
		iss >> str;
		processes[i].totalPageFramesOnDisk = stoi(str);
		AllocateIntoDisk(&processes[i]);
	}

	//	Loop through each instruction and perform parse
	for (size_t j = totalProcesses; j < instructions.size(); j++)
	{
		string instruction = instructions[j];

		for (size_t i = 0; i < totalProcesses; i++)
		{
			string processIDtoString = to_string(processes[i].processID);
			if (instruction.find(processIDtoString) != string::npos)
			{
				//	Shave currentLine so it only contains the address string
				string addressString = instruction.erase(0, processIDtoString.length() + 1);

				//	Convert address string to an int
				char *cstr = new char[addressString.length() + 1];
				strcpy(cstr, addressString.c_str());
				char *pLine;
unsigned int addressToInt = (unsigned int)strtol(cstr, &pLine, 0);
				if (((int)addressToInt) < 0)
				{
					continue;
				}
				//	Parse address for our Displacement, Page, and Segment
				int displacement = addressToInt & ((1 << (int)log2(pageSize)) - 1);
				int pageID = (addressToInt >> (int)log2(pageSize)) & ((1 << (int)log2(segmentLength)) - 1);
				int segmentID = addressToInt >> ((int)log2(pageSize) + (int)log2(segmentLength));

				cout<<"Attempting page access with Process , Segment , Page , Segment Length .\n"<< i, segmentID, pageID, processes[i].addressSpace.segmentTables[0].segments.size();
				//	Attempt to page access
				Page *pPageAccessing = &processes[i].addressSpace.segmentTables[0].segments[segmentID].pageTable.pages[pageID];

				Frame *pFrame = NULL;
				bool PageFault = false;
				//	if the accessing page is allocated, page fault
				if (pPageAccessing->isAllocated == false)
				{
					PageFault = true;

					cout << "\033[0;31m * Page fault \033[0m" << endl;
				}
				//	hit
				else if (!(framesInMainMemory[pPageAccessing->frameID].processID == processes[i].processID &&
					framesInMainMemory[pPageAccessing->frameID].pageID == pageID &&
					framesInMainMemory[pPageAccessing->frameID].segmentID == segmentID))
				{
					PageFault = true;
					cout << "\033[0;33m * Page fault (Was Replaced) \033[0m" << endl;
				}

				if (PageFault)
				{
					pagefault++;
					if (processes[i].addressSpace.LastPagesUsed.size() < pageFramePerProcess)
					{
						//	look for empty space in the main memory
						for (int k = 0; k < framesInMainMemory.size(); k++)
						{
							if (framesInMainMemory[k].isAllocated == false)
							{
								pFrame = &framesInMainMemory[k];
								cout << "\033[0;32m Found unallocated frame in main memory. \033[0m" << endl;

								break;
							}
						}
						if (pFrame == NULL)
						{
							cout<<"\033[0;31m *ERROR*, pFrame==NULL \033[0m\n";
							break;
						}
					}
					else //Replace Page
					{
						pFrame = &framesInMainMemory[processes[i].addressSpace.LastPagesUsed[0].frameID];
						processes[i].addressSpace.LastPagesUsed.pop_back();
						cout<<"\033[0;32m Replaced frame %d...\033[0m\n", pFrame->frameID;
					}
					//cout<<"ALlocating to Frame(%d)\n", pFrame->frameID);
					pFrame->isAllocated = true;

					pPageAccessing->frameID = pFrame->frameID;

					pFrame->processID = processes[i].processID;
					pFrame->pageID = pageID;
					pFrame->segmentID = segmentID;
					//cout<<"Setting Page(%d) isAllocated To True\n", pPageAccessing->pageID);
					pPageAccessing->isAllocated = true;
					processes[i].addressSpace.LastPagesUsed.push_back(*pPageAccessing);

					//	Add page into history
					History_Array.push_back(pPageAccessing);
				}
				else
				{
					//	Add page into history
					History_Array.push_back(pPageAccessing);
					hit++;
					cout<<"\033[0;32m Page Found. \033[0m\n";
				}

				break;
			}
		}

		/*cout << "History Array: [";
		/*for (size_t history = 0; history < History_Array.size(); history++)
		{
			if (history + 1 == History_Array.size())
				cout << History_Array[history]->frameID;
			else
				cout << History_Array[history]->frameID << ", ";
		}
		cout << "]" << endl;*/
	}
}
		



		
int main()
{
	int a;
	pagefault=0;hit=0;
	ifstream inputFile;
	//	Read, Evaluate, and Assign variables based in the input .txt file supplied by command argument
	inputFile.open("/home/ayush/Downloads/input.txt");
	if (inputFile.is_open()){
		string currentLine;

		//	Fetch next line & cache totalPages
		getline(inputFile, currentLine);
		sizeOfMainMemory = stoi(currentLine);

		//	Fetch next line & cache pageSize
		getline(inputFile, currentLine);
		pageSize = stoi(currentLine);
		
		//	Fetch next line & cache pageFramePerProcess
		getline(inputFile, currentLine);
		pageFramePerProcess = stoi(currentLine);

		//	Fetch next line & cache lookAheadwindow
		getline(inputFile, currentLine);
		lookAheadwindow = stoi(currentLine);

		//	Fetch next line & cache windowMin
		getline(inputFile, currentLine);
		windowMin = stoi(currentLine);

		//	Fetch next line & cache windowMax
		getline(inputFile, currentLine);
		windowMax = stoi(currentLine);

		//	Fetch next line & cache totalProcesses
		getline(inputFile, currentLine);
		totalProcesses = stoi(currentLine);

		//	Fetch next line & add to intypedef struction array
		while(getline(inputFile, currentLine))
{
		instructions.push_back(currentLine);	
		a++;
}
}


	//	Display statistics
	cout << "\n-------------------------	Parameters	------------------------\n" << endl;
	cout << "Total pages: \t\t\t" << sizeOfMainMemory << endl;
	cout << "Page size: \t\t\t" << pageSize << endl;
	cout << "Page frame per process: \t" << pageFramePerProcess << endl;
	cout << "Look ahead window: \t\t" << lookAheadwindow << endl;
	cout << "Window Min: \t\t\t" << windowMin << endl;
	cout << "Window Max: \t\t\t" << windowMax << endl;
	cout << "Total processes: \t\t" << totalProcesses << endl;
	cout << "No of lines : "<< a << endl;
	int z;
	cout << "Our Algorithms :"<<endl;
	cout << "1.FIFO_PageReplacement"<<endl;
	cout << "2.LRU_PageReplacement"<<endl;
	cout << "3.LFU_PageReplacement"<<endl;
	cout << "4.OPT_PageReplacement"<<endl;
	cout << "5.WS_PageReplacement"<<endl;
	cout << "Which algorithm u want to process ";
	cin >> z; 
	if(z==1)
	{
	cout << "\n-------------------------	FIFO Page Replacement	------------------------\n" << endl;
	FIFO_PageReplacement();

	}
	else if(z==2)
	{
	cout << "\n-------------------------	LRU Page Replacement	--------------------------\n"<<endl;
	LRU_PageReplacement();
	}
	else if(z==3)
	{
	cout << "\n-------------------------	LFU Page Replacement	--------------------------\n"<<endl;
	LFU_PageReplacement();
	}
	else if(z==4)
	{
	cout << "\n-------------------------	OPT Page Replacement	--------------------------\n"<<endl;
	OPT_PageReplacement();
	}
	else if(z==5)
	{
	cout << "\n-------------------------	WS Page Replacement	--------------------------\n"<<endl;
	WS_PageReplacement();
	}
	else
	cout<<"Please Choose a valid algorithm"<<endl;
	cout<<"No of page fault : "<<pagefault<<endl;
	cout<<"No of page hit : "<<hit<<endl;
	return 0;
	//YOU CAN ALSO CALCULATE WHICH ALGORITHM WILL BE BEST FOR THE GIVEN DATA/FILE BY COMPARING THE NO OF PAGE FAULTS IN ALL OF THE ALGO
	//WE WILL CHOOSE THE ALGO HAVING LESS PAGE FAULTS
}

	
				


