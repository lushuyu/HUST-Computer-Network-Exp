#include "stdafx.h"
#include "Global.h"
#include "StopWaitRdtSender.h"


StopWaitRdtSender::StopWaitRdtSender():base(0),expectSequenceNumberSend(0),waitingState(false)
{
}


StopWaitRdtSender::~StopWaitRdtSender()
{
}



bool StopWaitRdtSender::getWaitingState() {
	return waitingState;
}




bool StopWaitRdtSender::send(const Message &message) {
	if (this->waitingState) { //���ͷ����ڵȴ�ȷ��״̬
		return false;
	}
	if (init_flag == 1) {
		for (int i = 0; i < Seqlenth; i++) {
			this->packetWaitingAck[i].seqnum = -1;
		}
		init_flag = 0;
	}
	if (expectSequenceNumberSend < base + N) {

		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].acknum = -1; //���Ը��ֶ�
		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].seqnum = this->expectSequenceNumberSend;
		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].checksum = 0;
		memcpy(this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].payload, message.data, sizeof(message.data));
		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].checksum = pUtils->calculateCheckSum(this->packetWaitingAck[expectSequenceNumberSend % Seqlenth]);

		pUtils->printPacket("Sending content ", this->packetWaitingAck[expectSequenceNumberSend % Seqlenth]);
		if (base == expectSequenceNumberSend) {
			cout << "Sending timer " << endl;
			pns->startTimer(SENDER, Configuration::TIME_OUT, base);			//�������ͻ����з���ʱ��
		}
		pns->sendToNetworkLayer(RECEIVER, this->packetWaitingAck[expectSequenceNumberSend % Seqlenth]);								//����ģ�����绷����sendToNetworkLayer��ͨ������㷢�͵��Է�
		expectSequenceNumberSend++;
		cout << "expectSequenceNumberSend is " << expectSequenceNumberSend << endl;
		if (expectSequenceNumberSend == base + N) {
			this->waitingState = true;//����ȴ�״̬
		}
	}
	return true;
}

void StopWaitRdtSender::receive(const Packet& ackPkt) {
	//	if (this->waitingState == true) {//������ͷ����ڵȴ�ack��״̬�������´�������ʲô������
			//���У����Ƿ���ȷ
	int checkSum = pUtils->calculateCheckSum(ackPkt);

	//���У�����ȷ������ȷ�����=���ͷ��ѷ��Ͳ��ȴ�ȷ�ϵ����ݰ����
	if (checkSum == ackPkt.checksum && ackPkt.acknum >= base) {
		//this->expectSequenceNumberSend = 1 - this->expectSequenceNumberSend;			
		int old_base = base;

		pUtils->printPacket("Correct.", ackPkt);
		base = ackPkt.acknum + 1;
		for (int i = base +N; i < base + 8; i++) {
			int a = i % Seqlenth;
			packetWaitingAck[i % Seqlenth].seqnum = -1;
		}
		cout << "sliding window " << '[' << ' ';
		for (int i = base; i < base + N; i++) {
			if (packetWaitingAck[i % Seqlenth].seqnum == -1) {
				cout << '*' << ' ';
			}
			else {
				cout << packetWaitingAck[i % Seqlenth].seqnum << ' ';
			}
		}
		cout << ']' << endl;
		if (base == expectSequenceNumberSend)
		{
			cout << "All sent. Close timer." << endl;
			this->waitingState = false;
			pns->stopTimer(SENDER, old_base);	//�رն�ʱ��
		}
		else {
			pns->stopTimer(SENDER, old_base);//��û�����꣬�����ȴ�
			pns->startTimer(SENDER, Configuration::TIME_OUT, base);
			this->waitingState = false;
		}

	}
	else {
		if (checkSum != ackPkt.checksum) {
			cout << "ACK Broken" << endl;
		}
		else {
			cout << "Incorrect, waiting." << endl;
		}
	}
	//}	
}

void StopWaitRdtSender::timeoutHandler(int seqNum) {
	//Ψһһ����ʱ��,���迼��seqNum
	cout << "Sending timeout, N steps back." << endl;
	pns->stopTimer(SENDER, seqNum);
	pns->startTimer(SENDER, Configuration::TIME_OUT, seqNum);
	int i = base;
	do {
		cout << "Resending" << i << "." << endl;
		pUtils->printPacket("Sender's timer has expired. Resending the message", this->packetWaitingAck[i % Seqlenth]);
		pns->sendToNetworkLayer(RECEIVER, this->packetWaitingAck[i % Seqlenth]);			//���·������ݰ�
		i++;
	} while (i != expectSequenceNumberSend);


}
