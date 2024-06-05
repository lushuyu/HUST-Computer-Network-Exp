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
	if (this->waitingState) { //发送方处于等待确认状态
		return false;
	}
	if (init_flag == 1) {
		for (int i = 0; i < Seqlenth; i++) {
			this->packetWaitingAck[i].seqnum = -1;
		}
		init_flag = 0;
	}
	if (expectSequenceNumberSend < base + N) {

		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].acknum = -1; //忽略该字段
		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].seqnum = this->expectSequenceNumberSend;
		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].checksum = 0;
		memcpy(this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].payload, message.data, sizeof(message.data));
		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].checksum = pUtils->calculateCheckSum(this->packetWaitingAck[expectSequenceNumberSend % Seqlenth]);

		pUtils->printPacket("Sender sends message", this->packetWaitingAck[expectSequenceNumberSend % Seqlenth]);
		if (base == expectSequenceNumberSend) {
			cout << "Sender Start Timer" << endl;
			pns->startTimer(SENDER, Configuration::TIME_OUT, base);			//启动发送基序列方定时器
		}
		pns->sendToNetworkLayer(RECEIVER, this->packetWaitingAck[expectSequenceNumberSend % Seqlenth]);								//调用模拟网络环境的sendToNetworkLayer，通过网络层发送到对方
		expectSequenceNumberSend++;
		cout << "expectSequenceNumberSend is " << expectSequenceNumberSend << endl;
		if (expectSequenceNumberSend == base + N) {
			this->waitingState = true;//进入等待状态
		}
	}
	return true;
}

void StopWaitRdtSender::receive(const Packet& ackPkt) {

			//检查校验和是否正确
	int checkSum = pUtils->calculateCheckSum(ackPkt);

	//如果校验和正确，并且确认序号=发送方已发送并等待确认的数据包序号
	if (checkSum == ackPkt.checksum && ackPkt.acknum >= base) {

		int old_base = base;

		pUtils->printPacket("Sender received confirmation", ackPkt);
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
			cout << "Done. Close Timer." << endl;
			this->waitingState = false;
			pns->stopTimer(SENDER, old_base);	//关闭定时器
		}
		else {
			pns->stopTimer(SENDER, old_base);//还没接收完，继续等待
			pns->startTimer(SENDER, Configuration::TIME_OUT, base);
			this->waitingState = false;
		}

	}
	else {
		
		if (ackPkt.acknum == lastack ) {
			ACK_count++;
			if (ACK_count == 4) {
				cout << "Received 3 redundant ACKs, quickly retransmitting sequence numbers 快速" << ackPkt.acknum + 1 <<endl<<endl<< endl;
				pns->stopTimer(SENDER, ackPkt.acknum + 1);
				pns->startTimer(SENDER, Configuration::TIME_OUT, ackPkt.acknum + 1);
				pns->sendToNetworkLayer(RECEIVER, this->packetWaitingAck[base % Seqlenth]);
			}
		}
		else {
			lastack = ackPkt.acknum;
			ACK_count = 1;
		}
		if (checkSum != ackPkt.checksum) {
			cout << "Sender: ACK broken" << endl;
		}
		else {
			cout << "wrong ACK, keep waiting" << endl;
		}
	}
	//}	
}

void StopWaitRdtSender::timeoutHandler(int seqNum) {
	//唯一一个定时器,无需考虑seqNum
	cout << "timeout" << endl;
	pns->stopTimer(SENDER, seqNum);
	pns->startTimer(SENDER, Configuration::TIME_OUT, seqNum);
	cout << "resending" << seqNum << "." << endl;
	pUtils->printPacket("Sender's timer has expired, resend the message", this->packetWaitingAck[seqNum % Seqlenth]);
	pns->sendToNetworkLayer(RECEIVER, this->packetWaitingAck[seqNum % Seqlenth]);			//重新发送数据包



}
