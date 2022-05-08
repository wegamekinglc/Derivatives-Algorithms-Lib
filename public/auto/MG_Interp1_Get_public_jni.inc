
JNIEXPORT jobject JNICALL Java_runtime_Base_nativeInterp1_Get
    (JNIEnv* jenv, jobject obj, jobject j_f, jdoubleArray j_x)
{
    const char* argName = nullptr;
    try
    {
        argName = "f (input #1)";
        const Handle_<Interp1_> f = JNI::ToHandle<Interp1_>(jenv, j_f);
        argName = "x (input #2)";
        const Vector_<double> x = JNI::ToDoubleVector(jenv, j_x);
        argName = nullptr;

		Vector_<double> y;
        Interp1_Get(f, x, &y);
        static const jclass THE_CLASS = jenv->FindClass("runtime.Base/Interp1_Get_Outputs");
        REQUIRE(THE_CLASS, "Can't find output class");
        static const jmethodID THE_CONSTRUCTOR = jenv->GetMethodID(THE_CLASS, "<init>", "([D))V");
        REQUIRE(THE_CONSTRUCTOR, "Can't find output initializer");
		auto y_ret = JNI::CopyOut(jenv, y);
        return jenv->NewObject(THE_CLASS, THE_CONSTRUCTOR, y_ret);
    }
    catch (std::exception& e)
    {
        return JNI::Error(jenv, e.what(), argName);
    }
    catch (...)
    {
        return JNI::Error(jenv, "Unknown error", argName);
    }
}
