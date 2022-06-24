#include "ncbind/ncbind.hpp"

static iTJSDispatch2 *UnhandledExceptionFilterClosure = NULL;

typedef struct ExceptionFilterClosureData_
{
	PEXCEPTION_POINTERS exception;
	LONG *pres;
} ExceptionFilterClosureData;

static void TJS_USERENTRY TryReleaseUnhandledExceptionFilterClosure(void *data)
{
	(void)data;

	if (UnhandledExceptionFilterClosure != NULL)
	{
		UnhandledExceptionFilterClosure->Release();
		UnhandledExceptionFilterClosure = NULL;
	}
}

static bool TJS_USERENTRY CatchReleaseUnhandledExceptionFilterClosure(void *data, const tTVPExceptionDesc &desc)
{
	(void)data;
	(void)desc;

	return false;
}

static void ReleaseUnhandledExceptionFilterClosure(void)
{
	TVPDoTryBlock(&TryReleaseUnhandledExceptionFilterClosure, &CatchReleaseUnhandledExceptionFilterClosure, NULL, NULL);
}

static tTJSVariant CreateDicFromException(PEXCEPTION_RECORD exception_record)
{
	tTJSVariant result;
	tTJSVariant tmp_variant;

	iTJSDispatch2 *dict = TJSCreateDictionaryObject();
	if (dict != NULL)
	{
		if (exception_record != NULL)
		{
			tmp_variant = (tTVInteger)(exception_record->ExceptionCode);
			dict->PropSet(TJS_MEMBERENSURE, TJS_W("ExceptionCode"), NULL, &tmp_variant, dict);
			tmp_variant = (tTVInteger)(exception_record->ExceptionFlags);
			dict->PropSet(TJS_MEMBERENSURE, TJS_W("ExceptionFlags"), NULL, &tmp_variant, dict);
			if (exception_record->ExceptionRecord != NULL)
			{
				tmp_variant = CreateDicFromException(exception_record->ExceptionRecord);
				dict->PropSet(TJS_MEMBERENSURE, TJS_W("ExceptionRecord"), NULL, &tmp_variant, dict);
			}
			tmp_variant = (tTVInteger)(exception_record->NumberParameters);
			dict->PropSet(TJS_MEMBERENSURE, TJS_W("NumberParameters"), NULL, &tmp_variant, dict);
			iTJSDispatch2 *array = TJSCreateArrayObject();
			if (array != NULL)
			{
				for (DWORD i = 0; i < exception_record->NumberParameters; i += 1)
				{
					tmp_variant = (tTVInteger)(LONG_PTR)(exception_record->ExceptionInformation[i]);
					array->PropSetByNum(0, (tjs_int)i, &tmp_variant, array);
				}
				tmp_variant = array;
				array->Release();
			}
		}
		result = dict;
		dict->Release();
	}
	return result;
}


static void TJS_USERENTRY TryCallUnhandledExceptionFilterClosure(void *data)
{
	ExceptionFilterClosureData *pdata;

	pdata = (ExceptionFilterClosureData *)data;
	if (pdata != NULL && pdata->pres != NULL)
	{
		if (UnhandledExceptionFilterClosure != NULL)
		{
			tTJSVariant dicexception;
			tTJSVariant result;
			tTJSVariant *param = {&dicexception};

			if (pdata->exception != NULL)
			{
				dicexception = CreateDicFromException(pdata->exception->ExceptionRecord);
			}
			UnhandledExceptionFilterClosure->FuncCall(0, NULL, NULL, &result, 1, &param, UnhandledExceptionFilterClosure);
			if (result.Type() != tvtVoid)
			{
				*(pdata->pres) = result.AsInteger();
			}
		}
	}
}

static bool TJS_USERENTRY CatchCallUnhandledExceptionFilterClosure(void *data, const tTVPExceptionDesc &desc)
{
	(void)desc;
	ExceptionFilterClosureData *pdata;

	pdata = (ExceptionFilterClosureData *)data;
	if (pdata != NULL && pdata->pres != NULL)
	{
		*(pdata->pres) = EXCEPTION_CONTINUE_SEARCH;
	}

	return false;
}

static void CallUnhandledExceptionFilterClosure(ExceptionFilterClosureData *data)
{
	TVPDoTryBlock(&TryCallUnhandledExceptionFilterClosure, &CatchCallUnhandledExceptionFilterClosure, NULL, data);
}

static LONG WINAPI SelfUnhandledExceptionHandler(PEXCEPTION_POINTERS exception)
{
	LONG res;
	ExceptionFilterClosureData data;
	data.exception = exception;
	data.pres = &res;

	res = EXCEPTION_CONTINUE_SEARCH;
	CallUnhandledExceptionFilterClosure(&data);
	return res;
}

struct System
{
	static void setUnhandledExceptionFilter(tTJSVariant val)
	{
		ReleaseUnhandledExceptionFilterClosure();
		UnhandledExceptionFilterClosure = val.AsObject();
	}
};

NCB_ATTACH_FUNCTION(setUnhandledExceptionFilter, System, System::setUnhandledExceptionFilter);

static void SelfRegisterCallback(void)
{
	SetUnhandledExceptionFilter(SelfUnhandledExceptionHandler);
}

NCB_PRE_REGIST_CALLBACK(SelfRegisterCallback);


static void SelfUnregisterCallback(void)
{
	SetUnhandledExceptionFilter(NULL);
	ReleaseUnhandledExceptionFilterClosure();
}

NCB_POST_UNREGIST_CALLBACK(SelfUnregisterCallback);
