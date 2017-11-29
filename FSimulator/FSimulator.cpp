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

	void setEndDate(const int newEndDate) {
		m_endDate = newEndDate;
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
	{}

	Box(Box &&) = default;

	int numberOfQueuedTickets() const
	{
		return m_inboxTickets.size();
	}

	int speed() const
	{
		return m_speed;
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
	deque<TicketEntityPtr> m_doneTickets;
	deque<TicketEntityPtr> m_inboxTickets;
};


class PipelineController
{
public:
	PipelineController(deque<Box> boxes)
		: m_boxes(move(boxes)), m_currentDay(0)
	{}

	void stepForward()
	{
		feedInputBoxQueues();

		// launch boxes sequentially - virtually in parallel in the sim
		for (auto & box : m_boxes)
			box.perform();

		// logging/display only
		printBoxStates();

		// takes last box "done" work to put it in global throughput container
		updateGlobalThroughput();

		cout << "cumulated throughput: " << m_cumulatedThroughput.size() << endl;

		// corresponding to the current day
		m_currentDay++;
	}

private:
	void feedInputBoxQueues()
	{
		// transfer each box "done" work to "inputbox" queues of next boxes
		for (unsigned int i = 1; i < m_boxes.size(); ++i)
		{
			deque<TicketEntityPtr> previousDoneTickets = popAll(m_boxes[i - 1].doneTickets());
			move(previousDoneTickets.begin(), previousDoneTickets.end(), back_inserter(m_boxes[i].inboxTickets()));
		}

		if (m_boxes.empty())
			return;

		// feed new work in first box
		auto & firstBox = m_boxes.front();
		for (int i = 0; i < firstBox.speed(); ++i) {
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
		for (TicketEntityPtr& ticket : outputtedTickets)
			ticket->setEndDate(m_currentDay);
	}

	void printBoxStates() const
	{
		cout << "system state:" << endl;

		int i = 0;
		for (const auto & box : m_boxes)
		{
			cout << "box " << i << " (speed: " << box.speed() << ")" << endl;
			cout << "\t queue: " << box.numberOfQueuedTickets() << endl;
			cout << "\t in progress: " << box.doneTickets().size() << endl << endl;
			++i;
		}
	}

	deque<Box> m_boxes;
	deque<TicketEntityPtr> m_cumulatedThroughput;
	int m_lastDayThroughput;
	int m_currentDay;
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
				cout << "What do you want to do? (q: quit, anything else: continue)" << endl;
			}

			getInputLine(cin, ans);
		}

	}

	void runCommand(const string & command)
	{
		switch (str2int(command.c_str()))
		{
		default:
			m_pipelineController.stepForward();
		}
	}

private:
	PipelineController & m_pipelineController;

};


int main()
{
	cout << "Number of boxes? " << endl;
	int nbBoxes = waitForPositiveIntegerFromStdIn();


	deque<Box> boxQueue;
	for (int i = 0; i < nbBoxes; ++i)
	{
		cout << "Speed for box " << i << "? " << endl;
		int speed = waitForPositiveIntegerFromStdIn();
		boxQueue.push_back(Box(speed));
	}

	cin.ignore(numeric_limits<streamsize>::max(), '\n'); //flush stdin buffer

	PipelineController pipelineController(move(boxQueue));
	CommandLoop commandLoop(pipelineController);

	commandLoop.exec();


	return 0;
}