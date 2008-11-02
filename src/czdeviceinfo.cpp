/*!
	\file czdeviceinfo.cpp
	\brief CUDA device information source file.
	\author AG
*/

#include <QDebug>

#include "czdeviceinfo.h"

/*!
	\class CZUpdateThread
	\brief This class implements performance data update procedure.
*/

/*!
	\brief Creates the performance data update thread.
*/
CZUpdateThread::CZUpdateThread(
	CZCudaDeviceInfo *info,
	QObject *parent			/*!< Parent of the thread. */
)	: QThread(parent) {

	abort = false;
	testRunning = false;
	deviceReady = false;
	this->info = info;
	index = -1;

	qDebug() << "Thread created";
}

/*!
	\brief Terminates the performance data update thread.
	This function waits util performance test will be over.
*/
CZUpdateThread::~CZUpdateThread() {

	mutex.lock();
	deviceReady = true;
	readyForWork.wakeOne();
	abort = true;
	newLoop.wakeOne();
	testRunning = true;
	testStart.wakeAll();
	testRunning = false;
	testFinish.wakeAll();
	mutex.unlock();

	wait();

	qDebug() << "Thread is done";
}

/*!
	\brief Push performance test.
*/
void CZUpdateThread::testPerformance(
	int index			/*!< Index of device in list. */
) {
	qDebug() << "Rising update action for device" << index;

	mutex.lock();
	this->index = index;
	if(!isRunning()) {
		start();
		qDebug() << "Waiting for device is ready...";
	}

	while(!deviceReady)
		readyForWork.wait(&mutex);

	newLoop.wakeOne();
	mutex.unlock();
}

/*!
	\brief Wait for performance test results.
*/
void CZUpdateThread::waitPerformance() {

	testPerformance(-1);

	qDebug() << "Waiting for results...";

	mutex.lock();
	qDebug() << "Waiting for beginnig of test...";
	while(!testRunning)
		testStart.wait(&mutex);
	qDebug() << "Waiting for end of test...";
	while(testRunning)
		testFinish.wait(&mutex);
	mutex.unlock();

	qDebug() << "Got results!";
}

/*!
	\brief Main work function of the thread.
*/
void CZUpdateThread::run() {

	qDebug() << "Thread started";

	info->prepareDevice();

	mutex.lock();
	deviceReady = true;
	readyForWork.wakeAll();

	forever {

		qDebug() << "Waiting for new loop...";
		newLoop.wait(&mutex);
		index = this->index;
		mutex.unlock();

		qDebug() << "Thread loop started";

		if(abort) {
			mutex.lock();
			break;
		}

		mutex.lock();
		testRunning = true;
		testStart.wakeAll();
		mutex.unlock();

		info->updateInfo();

		mutex.lock();
		testRunning = false;
		testFinish.wakeAll();
		mutex.unlock();

		if(index != -1)
			emit testedPerformance(index);

		if(abort) {
			mutex.lock();
			break;
		}

		mutex.lock();
	}

	deviceReady = false;
	mutex.unlock();

	info->cleanDevice();
}

/*!
	\class CZCudaDeviceInfo
	\brief This class implements a container for CUDA-device information.
*/

/*!
	\brief Creates CUDA-device information container.
*/
CZCudaDeviceInfo::CZCudaDeviceInfo(
	int devNum,
	QObject *parent
) 	: QObject(parent) {
	memset(&_info, 0, sizeof(_info));
	_info.num = devNum;
	readInfo();
	_thread = new CZUpdateThread(this, this);
	connect(_thread, SIGNAL(testedPerformance(int)), this, SIGNAL(testedPerformance(int)));
	_thread->start();
}

/*!
	\brief Destroys cuda information container.
*/
CZCudaDeviceInfo::~CZCudaDeviceInfo() {
	delete _thread;
}

/*!
	\brief This function reads CUDA-device basic information.
	\return \a 0 in case of success, \a -1 in case of error.
*/
int CZCudaDeviceInfo::readInfo() {
	return CZCudaReadDeviceInfo(&_info, _info.num);
}

/*!
	\brief This function prepare some buffers for budwidth tests.
	\return \a 0 in case of success, \a -1 in case of error.
*/
int CZCudaDeviceInfo::prepareDevice() {
	return CZCudaPrepareDevice(&_info);
}

/*!
	\brief This function updates CUDA-device performance information.
	\return \a 0 in case of success, \a -1 in case of error.
*/
int CZCudaDeviceInfo::updateInfo() {
	int r;
	r = CZCudaCalcDeviceBandwidth(&_info);
	if(r == -1)
		return r;
	return CZCudaCalcDevicePerformance(&_info);
}

/*!
	\brief This function cleans buffers used for bandwidth tests.
	\return \a 0 in case of success, \a -1 in case of error.
*/
int CZCudaDeviceInfo::cleanDevice() {
	return CZCudaCleanDevice(&_info);
}

/*!
	\brief Returns pointer to inforation structure.
*/
struct CZDeviceInfo &CZCudaDeviceInfo::info() {
	return _info;
}

/*!
	\brief Returns pointer to update thread.
*/
//CZUpdateThread *CZCudaDeviceInfo::thread() {
//	return _thread;
//}


/*!
	\brief Push performance test in thread.
*/
void CZCudaDeviceInfo::testPerformance(
	int index			/*!< Index of device in list. */
) {
	_thread->testPerformance(index);
}

/*!
	\brief Wait for performance test results.
*/
void CZCudaDeviceInfo::waitPerformance() {
	_thread->waitPerformance();
}