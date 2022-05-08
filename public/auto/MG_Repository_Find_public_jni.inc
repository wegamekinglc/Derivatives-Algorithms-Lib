
JNIEXPORT jobject JNICALL Java_runtime_Base_nativeRepository_Find
    (JNIEnv* jenv, jobject obj, jstring j_match)
{
    const char* argName = nullptr;
    try
    {
        argName = "match (input #1)";
        const String_ match = JNI::ToString(jenv, j_match);
        argName = nullptr;

		Vector_<Handle_<Storable_>> objects;
        Repository_Find(match, &objects);
        static const jclass THE_CLASS = jenv->FindClass("runtime.Base/Repository_Find_Outputs");
        REQUIRE(THE_CLASS, "Can't find output class");
        static const jmethodID THE_CONSTRUCTOR = jenv->GetMethodID(THE_CLASS, "<init>", "([LDA/Handle;))V");
        REQUIRE(THE_CONSTRUCTOR, "Can't find output initializer");
		auto objects_ret = JNI::CopyOut(jenv, objects);
        return jenv->NewObject(THE_CLASS, THE_CONSTRUCTOR, objects_ret);
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
