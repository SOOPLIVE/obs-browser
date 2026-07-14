#pragma once

#include <qstring.h>
#include <qpointer.h>
#include <qmetatype.h>

class QCefQuery {
public:
	QCefQuery(){}
	QCefQuery(QString req, int64_t query)
		:reqeust_(req), id_(query), restult_(false), error_(0)
	{
	}
	QCefQuery(const QCefQuery &other)
	{
		reqeust_ = other.reqeust_;
		id_ = other.id_;
		restult_ = other.restult_;
		response_ = other.response_;
		error_ = other.error_;
	}
	QCefQuery &operator=(const QCefQuery &other)
	{
		reqeust_ = other.reqeust_;
		id_ = other.id_;
		restult_ = other.restult_;
		response_ = other.response_;
		error_ = other.error_;
		return *this;
	}
	~QCefQuery() {}

	const QString reqeust() const
	{
		return reqeust_;
	}
	const int64_t id() const
	{
		return id_;
	}
	const QString response() const
	{
		return response_;
	}
	const bool result() const
	{
		return restult_;
	}
	const int error() const
	{
		return error_;
	}
	//
	void setResponseResult(bool success,
			       const QString &response,
			       int error = 0) const
	{
		restult_ = success;
		response_ = response;
		error_ = error;
	}

private:
	int64_t id_ = -1;
	QString reqeust_;
	mutable QString response_;
	mutable bool restult_ = false;
	mutable int error_ = 0;
	static int typeid_;
};
//
Q_DECLARE_METATYPE(QCefQuery);
