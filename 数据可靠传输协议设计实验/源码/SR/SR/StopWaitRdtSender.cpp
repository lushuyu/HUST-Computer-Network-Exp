#include "stdafx.h"
#include "Global.h"
#include "StopWaitRdtSender.h"


StopWaitRdtSender::StopWaitRdtSender():base(0),expectSequenceNumberSend(0),waitingState(false)
{
	for (int i = 0; i < Seqlenth; i++) {
		ACKFlags[i] = false;
	}
	
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

	if (expectSequenceNumberSend < base + N) {

		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].acknum = -1; //忽略该字段
		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].seqnum = this->expectSequenceNumberSend;
		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].checksum = 0;
		memcpy(this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].payload, message.data, sizeof(message.data));
		this->packetWaitingAck[expectSequenceNumberSend % Seqlenth].checksum = pUtils->calculateCheckSum(this->packetWaitingAck[expectSequenceNumberSend % Seqlenth]);
		ACKFlags[expectSequenceNumberSend % Seqlenth] = false;

		pUtils->printPacket("Sender sends message", this->packetWaitingAck[expectSequenceNumberSend % Seqlenth]);
		
		cout << "seq " << expectSequenceNumberSend << "Sender Start Timer" << endl;
		pns->startTimer(SENDER, Configuration::TIME_OUT, expectSequenceNumberSend);			//启动发送基序列方定时器
		
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

	int checkSum = pUtils->calculateCheckSum(ackPkt);

	//如果校验和正确，并且确认序号=发送方已发送并等待确认的数据包序号
	if (checkSum == ackPkt.checksum) {
		pUtils->printPacket("Sender received confirmation", ackPkt);
		for (int i = base + N; i < base + 8; i++) {

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
		if (ackPkt.acknum == base) {
			cout << "ACK number" << ackPkt.acknum << "'s ACK" << endl;
			pns->stopTimer(SENDER, ackPkt.acknum);
			ACKFlags[base % Seqlenth] = true;
			while (ACKFlags[base % Seqlenth]) {
				ACKFlags[base++ % Seqlenth] = false;
			}
			waitingState = false;
		}
		else if (ackPkt.acknum > base && !ACKFlags[ackPkt.acknum % Seqlenth]) {
			cout << "ACK number is" << ackPkt.acknum << "'s ACK" << endl;
			pns->stopTimer(SENDER, ackPkt.acknum);
			ACKFlags[ackPkt.acknum % Seqlenth] = true;
		}
		else
		{
			cout << "wrong ACK, keep waiting" << endl;
		}

	}
	else cout << "ACK broken" << endl;
	
}

void StopWaitRdtSender::timeoutHandler(int seqNum) {
	//唯一一个定时器,无需考虑seqNum
	cout << "send timeout" << endl;
	pns->stopTimer(SENDER, seqNum);
	pns->startTimer(SENDER, Configuration::TIME_OUT, seqNum);
	cout << "resending " << seqNum << " message " << endl;
	pUtils->printPacket(" Sender's timer has expired, resend the message", this->packetWaitingAck[seqNum % Seqlenth]);
	pns->sendToNetworkLayer(RECEIVER, this->packetWaitingAck[seqNum % Seqlenth]);			//重新发送数据包

}
