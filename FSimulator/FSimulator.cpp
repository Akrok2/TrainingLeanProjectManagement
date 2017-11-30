// FSimulator.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

// FSimulator.cpp : Defines the entry point for the console application.
//

#include <memory>
#include <deque>
#include <iostream>
#include <string>
#include <limits>
#include <iterator>
#include "textTable.h"
#include <algorithm>

using namespace std;


template<class T>
T pop(deque<T> &queue)
{
	T ret = move(queue.front());
	queue.pop_front();
	return move(ret);
}


template<class T>
deque<T> popAll(deque<T> &queue)
{
	deque<T> ret;
	ret.swap(queue);
	return move(ret);
}


class Ticket
{
public:
	Ticket() {}
	Ticket(int startDate) 
	: m_startDate(startDate), m_endDate(9999)
	{}
	Ticket(Ticket &&) = delete;

	void setEndDate(const int newEndDate) 
	{
		m_endDate = newEndDate;
	}

	int getEndDate(void)
	{
		return m_endDate;
	}

	int getStartDate(void)
	{
		return m_startDate;
	}

private:
	int m_startDate;
	int m_endDate;
};


using TicketEntityPtr = unique_ptr<Ticket>;


TicketEntityPtr make_ticket(const int startDate)
{
	return make_unique<Ticket>(startDate);
}


class Box
{
public:
	Box(int speed)
		: m_speed(speed)
		, m_wipLimit(std::numeric_limits<int>::max())
	{}

	Box(Box &&) = default;

	int numberOfQueuedTickets() const
	{
		return m_inboxTickets.size();
	}

	void setSpeed(int speed)
	{
		m_speed = speed;
	}

	int speed() const
	{
		return m_speed;
	}

	void setWipLimit(int value)
	{
		m_wipLimit = value;
	}

	int wipLimit() const
	{
		return m_wipLimit;
	}

	deque<TicketEntityPtr> &inboxTickets()
	{
		return m_inboxTickets;
	}

	const deque<TicketEntityPtr> &inboxTickets() const
	{
		return m_inboxTickets;
	}

	deque<TicketEntityPtr> &doneTickets()
	{
		return m_doneTickets;
	}

	const deque<TicketEntityPtr> &doneTickets() const
	{
		return m_doneTickets;
	}

	// Runs for a day
	void perform()
	{
		for (int i = 0; i < m_speed && !m_inboxTickets.empty(); ++i)
			m_doneTickets.push_back(pop(m_inboxTickets));
	}

private:
	int m_speed;
	int m_wipLimit;
	deque<TicketEntityPtr> m_doneTickets;
	deque<TicketEntityPtr> m_inboxTickets;
};


class PipelineController
{
public:
	PipelineController(deque<Box> boxes)
		: m_boxes(move(boxes))
		, m_currentDay(0)
	{}

	void stepForward()
	{
		auto outputQueue = pullQueuesFromRightToLeft();
		int dailyThoughput = outputQueue.size();
		move(outputQueue.begin(), outputQueue.end(), back_inserter(m_cumulatedThroughput));


		/*int lastDayCumulatedThroughput = m_cumulatedThroughput.size();
		int lastDayThoughputToPrint = m_lastDayThroughput;*/

		feedFirstBox();

		// launch boxes sequentially - virtually in parallel in the sim
		for (auto & box : m_boxes)
			box.perform();

		// logging/display only
		printBoxStates();

		const int totalWip = computeTotalWip();

		// takes last box "done" work to put it in global throughput container
		//updateGlobalThroughput();

		//m_lastDayThroughput = m_cumulatedThroughput.size() - lastDayCumulatedThroughput;

		// compute the cycle time
		computeCycleTime();

		// compute the bottleneck (first box starting from the end with a positive number of queued tickets)
		const int bottleNeckIndex = computeIndexOfBottleneck();

		cout << "throughput: " << dailyThoughput << endl;
		cout << "cumulated throughput: " << m_cumulatedThroughput.size() << endl;
		cout << "cycle time: " << m_cycleTime << endl;
		if (bottleNeckIndex != std::numeric_limits<int>::max())
			cout << "Bottleneck -> box " << bottleNeckIndex << endl;
		else
			cout << "No bottleneck " << endl;
		cout << "Total WIP : " << totalWip << endl;

		m_currentDay++;
	}

	void setSpeed(int boxIndex, int boxSpeed)
	{
		m_boxes.at(boxIndex).setSpeed(boxSpeed);
	}

	void setWipLimit(int boxIndex, int boxWipLimit)
	{
		m_boxes.at(boxIndex).setWipLimit(boxWipLimit);
	}

private:

	int computeTotalWip(void)
	{
		int totalWip(0);
		for (const Box& box : m_boxes)
		{
			totalWip += box.numberOfQueuedTickets() + box.doneTickets().size();
		}

		return totalWip;
	}
	

	void computeCycleTime(void)
	{
		int sumQueues = 0;
		for (const Box& box : m_boxes)
		{
			const double tempResult = ((double)box.numberOfQueuedTickets() / (double)box.speed());
			sumQueues += std::ceil(tempResult);
		}

		m_cycleTime = m_boxes.size() + sumQueues;
	}

	int computeIndexOfBottleneck(void)
	{
		int i = 0;
		int indexOfBottleneck = std::numeric_limits<int>::max();
		for (const Box& box : m_boxes)
		{
			if (box.numberOfQueuedTickets() > 0)
				indexOfBottleneck = i;
			i++;
		}

		return indexOfBottleneck;
	}

	deque<TicketEntityPtr> pullQueuesFromRightToLeft()
	{
		const int lastBoxIndex = m_boxes.size() - 1;
		int i = lastBoxIndex;
		deque<TicketEntityPtr> throughput;
		for (auto it = m_boxes.rbegin(); it != m_boxes.rend(); ++it)
		{	
			if (i == lastBoxIndex) {
				deque<TicketEntityPtr> previousDoneTickets = popAll(m_boxes[i].doneTickets());
				move(previousDoneTickets.begin(), previousDoneTickets.end(), back_inserter(throughput));
			}
			else  {
				int nextBoxCurrentWIP = m_boxes[i + 1].doneTickets().size() + m_boxes[i + 1].numberOfQueuedTickets();
				int currentBoxDoneTickets = m_boxes[i].doneTickets().size();
				int insertableTickets = std::min(m_boxes[i + 1].wipLimit(), currentBoxDoneTickets);

				for (int j = 0; j < insertableTickets; ++j) {
					auto ticket = pop(m_boxes[i].doneTickets());
					m_boxes[i + 1].inboxTickets().push_back(std::move(ticket));
				}
			}

			--i;
		}
		return move(throughput);
	}

	void feedFirstBox()
	{
		if (m_boxes.empty())
			return;

		int ticketsToFeed = m_boxes[0].speed();
		int firstBoxCurrentWIP = m_boxes[0].doneTickets().size() + m_boxes[0].numberOfQueuedTickets();
		int roomAvailable = m_boxes[0].wipLimit() - firstBoxCurrentWIP;
		ticketsToFeed = std::min(ticketsToFeed, roomAvailable);

		// feed new work in first box
		auto & firstBox = m_boxes.front();
		for (int i = 0; i < ticketsToFeed; ++i) {
			firstBox.inboxTickets().push_back(make_ticket(m_currentDay));
		}
	}

	// clear work from last box to update global throughput
	void updateGlobalThroughput()
	{
		if (m_boxes.empty())
			return;

		deque<TicketEntityPtr> outputtedTickets = popAll(m_boxes.back().doneTickets());
		move(outputtedTickets.begin(), outputtedTickets.end(), back_inserter(m_cumulatedThroughput));

		// fill in the end date for all finished tickets
		for (TicketEntityPtr& ticket : m_cumulatedThroughput)
			ticket->setEndDate(m_currentDay);
	}

	void printBoxStates() const
	{
		TextTable t('-', '|', '+');

		cout << "********************** STEP " << m_currentDay << "**********************" << endl;

		t.add(" ");
		int i = 0;
		for (const auto & box : m_boxes)
		{
			t.add("box "+std::to_string(i)+" (speed: "+std::to_string(box.speed())+")");
			i++;
		}
		t.endOfRow();

		t.add("Queue");
		for (const auto & box : m_boxes)
		{
			t.add(std::to_string(box.numberOfQueuedTickets()));
		}
		t.endOfRow();

		t.add("In Progress");
		for (const auto & box : m_boxes)
		{
			t.add(std::to_string(box.doneTickets().size()));
		}
		t.endOfRow();

		t.add("WIP limits");
		for (const auto & box : m_boxes)
		{
			t.add(std::to_string(box.wipLimit()));
		}
		t.endOfRow();

		t.setAlignment(2, TextTable::Alignment::LEFT);
		std::cout << t;
	}

	deque<Box> m_boxes;
	deque<TicketEntityPtr> m_cumulatedThroughput;
	int m_lastDayThroughput;
	int m_currentDay;
	int m_cycleTime;
};


template<class input_stream>
void getInputLine(const input_stream & istream, string & output)
{
	getline(cin, output);
}


int waitForPositiveIntegerFromStdIn()
{
	int ret = -1;
	while (ret < 0 || cin.fail()) {
		if (cin.fail()) {
			cout << "Please enter an positive integer" << endl;
			cin.clear();
			cin.ignore(numeric_limits<streamsize>::max(), '\n');
		}
		cin >> ret;
	}
	return ret;
}


constexpr unsigned int str2int(const char* str, int h = 0)
{
	return !str[h] ? 5381 : (str2int(str, h + 1) * 33) ^ str[h];
}

class CommandLoop
{
public:
	CommandLoop(PipelineController &pipelineController)
		: m_pipelineController(pipelineController)
	{}

	void exec()
	{
		string ans = "y";

		while (ans != "q") {

			switch (str2int(ans.c_str()))
			{
			case str2int("q"):
				break;
			default:
				runCommand(ans);
				cout << "Please enter a command? (q: quit, d: details)" << endl;
			}

			getInputLine(cin, ans);
		}

	}

	void runCommand(const string & command)
	{
		switch (str2int(command.c_str()))
		{
		case str2int("d"):
			cout << "COMMAND LIST:" << endl;
			cout << "\td: display this command list" << endl;
			cout << "\ts: speed adjustment" << endl;
			cout << "\tw: wip limits adjustment" << endl;
			cout << "\tq: quit application" << endl;
			cout << "\tanything else: step one day forward" << endl;
			break;
		case str2int("s"):
			adjustSpeed();
			break;
		case str2int("w"):
			adjustWipLimits();
			break;
		default:
			m_pipelineController.stepForward();
		}
	}

	void adjustSpeed()
	{
		cout << "index of box you want to change: ";
		int boxIndex = waitForPositiveIntegerFromStdIn();	

		// TODO: check input range

		cout << "please enter new speed: ";
		int newSpeed = waitForPositiveIntegerFromStdIn();

		m_pipelineController.setSpeed(boxIndex, newSpeed);
	}

	void adjustWipLimits()
	{
		cout << "index of box you want to change: ";
		int boxIndex = waitForPositiveIntegerFromStdIn();

		// TODO: check input range

		cout << "please enter new wip limit: ";
		int newWipLimit = waitForPositiveIntegerFromStdIn();

		m_pipelineController.setWipLimit(boxIndex, newWipLimit);
	}

private:
	PipelineController & m_pipelineController;

};


int main()
{
	cout << "Number of boxes: ";
	int nbBoxes = waitForPositiveIntegerFromStdIn();
	cout << endl;

	deque<Box> boxQueue;
	for (int i = 0; i < nbBoxes; ++i)
	{
		cout << "Speed for box " << i << ": ";
		int speed = waitForPositiveIntegerFromStdIn();
		boxQueue.push_back(Box(speed));
	}

	cin.ignore(numeric_limits<streamsize>::max(), '\n'); //flush stdin buffer

	PipelineController pipelineController(move(boxQueue));
	CommandLoop commandLoop(pipelineController);

	commandLoop.exec();


	return 0;
}