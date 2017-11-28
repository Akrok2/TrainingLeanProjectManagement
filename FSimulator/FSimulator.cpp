// FSimulator.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <memory>
#include <queue>
#include <iostream>
#include <vector>

using namespace std;

class Ticket
{
public:
	Ticket() {}
	Ticket(Ticket &&) = delete;
};

using TicketEntityPtr = unique_ptr<Ticket>;

TicketEntityPtr make_ticket()
{
	return make_unique<Ticket>();
}


class Box
{
public:
	Box(int speed)
		: m_speed(speed)
	{}

	int numberOfQueuedTickets() const
	{
		return m_inboxTickets.size();
	}

	int numberOfDoneTickets() const
	{
		return m_doneTickets.size();
	}

	int speed() const
	{
		return m_speed;
	}

	void enqueueIncomingTicket(TicketEntityPtr ticket)
	{
		m_inboxTickets.push(std::move(ticket));
	}

	void perform()
	{
		for (int i = 0; i < m_speed && !m_inboxTickets.empty(); ++i)
		{
			TicketEntityPtr ticketBeingProcessed = std::move(m_inboxTickets.front());
			m_inboxTickets.pop();
			m_doneTickets.push(std::move(ticketBeingProcessed));
		}
	}

	void updateQueues(Box & previousBox)
	{
		while (!previousBox.m_doneTickets.empty())
		{
			TicketEntityPtr ticketToMove = std::move(previousBox.m_doneTickets.front());
			previousBox.m_doneTickets.pop();
			enqueueIncomingTicket(std::move(ticketToMove));
		}
	}

private:
	int m_speed;
	queue<TicketEntityPtr> m_doneTickets;
	queue<TicketEntityPtr> m_inboxTickets;
};

class Pipeline
{
public:
	Pipeline(std::vector<Box> boxes)
		: m_boxes(std::move(boxes))
	{}

	void stepForward()
	{
		auto & firstBox = m_boxes.front();
		for (int i = 0; i < firstBox.speed(); ++i) {
			firstBox.enqueueIncomingTicket(make_ticket());
		}

		for (auto & box : m_boxes)
		{
			box.perform();
		}

		cout << "system state:" << endl;
		
		int i = 0;
		for (const auto & box : m_boxes)
		{
			cout << "box " << i << " (speed: " << box.speed() << " )" << endl;
			cout << "\t queue: " << box.numberOfQueuedTickets() << endl;
			cout << "\t in progress: " << box.numberOfDoneTickets() << endl << endl;		
			++i;
		}

		int throughPut = 0;
		cout << "Throughput: " << throughPut << endl;

		// end of the day
		for (int i = 1; i < m_boxes.size(); ++i)
		{
			m_boxes[i].updateQueues(m_boxes[i - 1]);
		}
	}

private:
	std::vector<Box> m_boxes;
};


int main()
{
	cout << "Number of boxes? " << endl;
	int nbBoxes = 0;
	cin >> nbBoxes;


	vector<Box> boxQueue;
	for (int i = 0; i < nbBoxes; ++i)
	{
		cout << "Speed for box " << i <<" ? " << endl;
		int speed = 0;
		cin >> speed;
		boxQueue.push_back(Box(speed));
	}

	Pipeline p(std::move(boxQueue));

	p.stepForward();

	while(true) {
		cout << "Do you want to continue (enter 0 for no or 1 for yes) ?" << endl;
		int ans = -1;
		cin >> ans;

		if (ans == 0)
		{
			return 0;
		}
		else
		{
			p.stepForward();
		}
	}
	


    return 0;
}

