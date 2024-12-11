#ifndef FEEDBACK_RECORD
#define FEEDBACK_RECORD

struct Feedback
{
    int feedbackID;
    int account; // Account number of the customer
    char name[25];
    char message[200];
};

#endif