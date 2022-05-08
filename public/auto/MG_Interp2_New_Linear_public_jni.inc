
JNIEXPORT jobject JNICALL Java_runtime_Base_nativeInterp2_New_Linear
    (JNIEnv* jenv, jobject obj, jstring j_name, jdoubleArray j_x, jdoubleArray j_y, jobjectArray j_z)
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
        argName = "z (input #4)";
        const Matrix_<double> z = JNI::ToDoubleMatrix(jenv, j_z);
        argName = nullptr;

		Handle_<Interp2_> f;
        Interp2_New_Linear(name, x, y, z, &f);
        static const jclass THE_CLASS = jenv->FindClass("runtime.Base/Interp2_New_Linear_Outputs");
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
