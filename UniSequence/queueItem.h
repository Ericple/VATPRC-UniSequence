#pragma once

using namespace std;

struct Extra
{
	string cid, departure, arrival;
};

struct QueueItem
{
	int status; string callsign; Extra extra;
};