
JNIEXPORT jobject JNICALL Java_runtime_Bones_nativeRepository_Size
    (JNIEnv* jenv, jobject obj)
{
    const char* argName = nullptr;
    try
    {

        argName = nullptr;

		int size;
        Repository_Size(&size);
        static const jclass THE_CLASS = jenv->FindClass("runtime.Bones/Repository_Size_Outputs");
        REQUIRE(THE_CLASS, "Can't find output class");
        static const jmethodID THE_CONSTRUCTOR = jenv->GetMethodID(THE_CLASS, "<init>", "(I)V");
        REQUIRE(THE_CONSTRUCTOR, "Can't find output initializer");
		auto size_ret = JNI::CopyOut(jenv, size);
        return jenv->NewObject(THE_CLASS, THE_CONSTRUCTOR, size_ret);
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
