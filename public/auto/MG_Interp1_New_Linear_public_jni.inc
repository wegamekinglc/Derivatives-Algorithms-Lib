
JNIEXPORT jobject JNICALL Java_runtime_Base_nativeInterp1_New_Linear
    (JNIEnv* jenv, jobject obj, jstring j_name, jdoubleArray j_x, jdoubleArray j_y)
{
    const char* argName = nullptr;
    try
    {
        argName = "name (input #1)";
        const String_ name = JNI::ToString(jenv, j_name);
        argName = "x (input #2)";
        const Vector_<double> x = JNI::ToDoubleVector(jenv, j_x);
        argName = "y (input #3)";
        const Vector_<double> y = JNI::ToDoubleVector(jenv, j_y);
        argName = nullptr;

		Handle_<Interp1_> f;
        Interp1_New_Linear(name, x, y, &f);
        static const jclass THE_CLASS = jenv->FindClass("runtime.Base/Interp1_New_Linear_Outputs");
        REQUIRE(THE_CLASS, "Can't find output class");
        static const jmethodID THE_CONSTRUCTOR = jenv->GetMethodID(THE_CLASS, "<init>", "(LDA/Handle;)V");
        REQUIRE(THE_CONSTRUCTOR, "Can't find output initializer");
		auto f_ret = JNI::CopyOut(jenv, f);
        return jenv->NewObject(THE_CLASS, THE_CONSTRUCTOR, f_ret);
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
