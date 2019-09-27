#include <iostream>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <vector>
#include <string>
#include <stdint.h>
#include <assert.h>

#define trace(what)(std::cout << '[' << __FILE__ << ':' << __LINE__ << "] " #what " = " << what << std::endl)

static void _fatal(const char *msg, const char *file, const int line)
{
	fprintf(stderr, "fatal: %s (%s:%d)\n", msg, file, line);
	exit(1);
}

static inline void print_errno()
{
	fprintf(stderr, "errno(%d) = %s\n", errno, strerror(errno));
}

#define fatal(msg)(_fatal((msg), __FILE__, __LINE__))

enum CHUNK_TYPE {
	C_STRUCT,
	C_REF,
	C_STRING,
	C_DOUBLE,
	C_INT,
	C_BOOL,
	C_ARRAY,
};

struct Chunk;

struct ChunkStruct {
	int nargs;
	std::string *name;
	std::vector<Chunk*> *args;
};

class Chunk {
public:
	Chunk() = delete;
	explicit Chunk(FILE *);
	~Chunk();
	void to_json(FILE *f);
	void to_html_ans(FILE *f);
	CHUNK_TYPE get_kind() const {
		return kind;
	}
	ChunkStruct as_struct() {
		assert(kind == C_STRUCT);
		return sto_struct;
	}
	Chunk *as_ref() {
		assert(kind == C_REF);
		return sto_ref;
	}
	std::string *as_string() {
		assert(kind == C_STRING);
		return sto_string;
	}
	double as_double() {
		assert(kind == C_DOUBLE);
		return sto_double;
	}
	int as_int() {
		assert(kind == C_INT);
		return sto_int;
	}
	bool as_bool() {
		assert(kind == C_BOOL);
		return sto_bool;
	}
	std::vector<Chunk *> *as_array() {
		assert(kind == C_ARRAY);
		return sto_array;
	}
	void to_struct(ChunkStruct value) {
		kind = C_STRUCT;
		sto_struct = value;
	}
	void to_ref(Chunk *value) {
		kind = C_REF;
		sto_ref = value;
	}
	void to_string(std::string *value) {
		kind = C_STRING;
		sto_string = value;
	}
	void to_double(double value) {
		kind = C_DOUBLE;
		sto_double = value;
	}
	void to_int(int value) {
		kind = C_INT;
		sto_int = value;
	}
	void to_bool(bool value) {
		kind = C_BOOL;
		sto_bool = value;
	}
	void to_array(std::vector<Chunk *> *value) {
		kind = C_ARRAY;
		sto_array = value;
	}
private:
	CHUNK_TYPE kind;
	union {
		ChunkStruct sto_struct;
		Chunk *sto_ref;
		std::string *sto_string;
		double sto_double;
		int sto_int;
		bool sto_bool;
		std::vector<Chunk*> *sto_array;
	};
};

template<typename T>
static T readf(FILE *f)
{
	T ret;
	if (fread(&ret, sizeof(T), 1, f) != 1) {
		print_errno();
		fatal("cannot read from file anymore");
	}
	return ret;
}

static Chunk *struct_defs;
static Chunk *root;

static void rep_str(std::string& subject, const std::string& search, const std::string& replace)
{
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
}

Chunk::Chunk(FILE *f)
{
	int16_t kint = readf<int16_t>(f);
	switch (kint) {
	case -12: {
		uint16_t index = readf<uint16_t>(f);
		ChunkStruct cs;
		Chunk *def = struct_defs->as_array()->at(index);
		cs.nargs = def->as_array()->at(0)->as_int();
		cs.args = new std::vector<Chunk *>();
		for (int i = 0; i < cs.nargs; ++i) {
			cs.args->push_back(new Chunk(f));
		}
		cs.name = def->as_array()->at(1)->as_string();
		to_struct(cs);
		break;
	}
	case -10:
		to_ref(new Chunk(f));
		break;
	case -6: {
		int len = readf<uint16_t>(f);
		to_string(new std::string());
		for (int i = 0; i < len; ++i) {
			as_string()->push_back(readf<char>(f));
			readf<char>(f);
		}
		break;
	}
	case -4:
		to_double(readf<double>(f));
		break;
	case -11:
		to_int(readf<uint32_t>(f));
		break;
	case -13:
		to_int((int)readf<uint64_t>(f));
		break;
	case -3:
		to_bool(false);
		break;
	case -2:
		to_bool(true);
		break;
	case -8: {
		int len = readf<uint16_t>(f);
		to_array(new std::vector<Chunk*>());
		for (int i = 0; i < len; ++i) {
			as_array()->push_back(new Chunk(f));
		}
		break;
	}
	case -9:
		to_array(new std::vector<Chunk*>());
		break;
	case -5: {
		int len = readf<uint32_t>(f);
		to_string(new std::string());
		for (int i = 0; i < len; ++i) {
			as_string()->push_back(readf<char>(f));
			readf<char>(f);
		}
		break;
	}
	case -7: {
		int len = readf<uint32_t>(f);
		to_array(new std::vector<Chunk*>());
		for (int i = 0; i < len; ++i) {
			as_array()->push_back(new Chunk(f));
		}
		break;
	}
	case -1:
		to_ref(nullptr);
		break;
	default:
		to_int((uint16_t)kint);
	}
	if (get_kind() == C_STRING) {
		rep_str(*as_string(), "\"", "&quote;");
	}
}

Chunk::~Chunk()
{
}

void Chunk::to_json(FILE *f)
{
	switch (get_kind()) {
	case C_STRUCT: {
		fprintf(f, "[ \"struct %s\"", as_struct().name->c_str());
		for (auto item : *as_struct().args) {
			fprintf(f, ",");
			item->to_json(f);
		}
		fprintf(f, "]");
		break;
	}
	case C_REF:
		as_ref()->to_json(f);
		break;
	case C_STRING:
		fprintf(f, "\"%s\"", as_string()->c_str());
		break;
	case C_INT:
		fprintf(f, "%d", as_int());
		break;
	case C_DOUBLE:
		fprintf(f, "%lf", as_double());
		break;
	case C_BOOL:
		fprintf(f, "%s", as_bool() ? "true" : "false");
		break;
	case C_ARRAY:
		fprintf(f, "[ \"array\"");
		for (auto item : *as_array()) {
			fprintf(f, ",");
			item->to_json(f);
		}
		fprintf(f, "]");
		break;
	}
}

void Chunk::to_html_ans(FILE *f)
{
	switch (get_kind()) {
	case C_STRUCT: {
		auto args = as_struct().args;
		if (*as_struct().name == "MCQProbeSourceData") {
			fprintf(f, "question id = %d\n", args->at(0)->as_int());
			fprintf(f, "type = %s\n", args->at(1)->as_struct().name->c_str());
			fprintf(f, "text = %s\n", args->at(2)->as_string()->c_str());
			auto choices = args->at(3)->as_array();
			for (auto item : *choices) {
				if (*item->as_struct().name != "AnswerData")
					continue;
				auto behaviorResp = item->as_struct().args->at(0)->as_struct();
				if (*behaviorResp.name != "DynamicBehaviour")
					continue;
				auto behaviorCorr = item->as_struct().args->at(2)->as_struct();
				if (*behaviorCorr.name != "DynamicBehaviour")
					continue;
				bool correct = behaviorCorr.args->at(0)->as_ref()->as_double() != 0;
				fprintf(f, "[%c] %s\n", correct ? 'X' : ' ', behaviorResp.args->at(0)->as_ref()->as_string()->c_str());
			}
			fprintf(f, "\n");
		} else if (*as_struct().name == "FillInProbeSourceData") {
			fprintf(f, "question id = %d\n", args->at(0)->as_int());
			fprintf(f, "type = fill in\n");
			fprintf(f, "text = %s\n", args->at(1)->as_string()->c_str());
			auto list = args->at(2)->as_array()->at(0)->as_array();
			for (auto item : *list) {
				const char *word = item->as_struct().args->at(0)->as_struct().args->at(0)->as_ref()->as_string()->c_str();
				double points = item->as_struct().args->at(2)->as_struct().args->at(0)->as_ref()->as_double();
				const char *hint = item->as_struct().args->at(1)->as_struct().args->at(0)->as_ref()->as_string()->c_str();
				if (hint[0])
					fprintf(f, "[%.2lf] %s (%s)\n", points, word, hint);
				else
					fprintf(f, "[%.2lf] %s\n", points, word);
			}
			fprintf(f, "\n");
		} else if (*as_struct().name == "MatchingProbeSourceData") {
			fprintf(f, "question id = %d\n", args->at(0)->as_int());
			fprintf(f, "type = matching\n");
			fprintf(f, "text = %s\n", args->at(1)->as_string()->c_str());
			auto list = args->at(2)->as_array();
			for (auto item : *list) {
				auto data = item->as_struct().args;
				const char *a = data->at(0)->as_struct().args->at(0)->as_ref()->as_string()->c_str();
				const char *b = data->at(1)->as_struct().args->at(0)->as_ref()->as_string()->c_str();
				fprintf(f, "[X] %s -> %s\n", a, b);
			}
			fprintf(f, "\n");
		}
		break;
	}
	case C_REF:
		as_ref()->to_html_ans(f);
		break;
	case C_STRING:
		fprintf(f, "\"%s\"", as_string()->c_str());
		break;
	case C_INT:
		fprintf(f, "%d", as_int());
		break;
	case C_DOUBLE:
		fprintf(f, "%lf", as_double());
		break;
	case C_BOOL:
		fprintf(f, "%s", as_bool() ? "true" : "false");
		break;
	case C_ARRAY:
		for (auto item : *as_array()) {
			item->to_html_ans(f);
		}
		break;
	}
}

static inline long calc_offset(long from)
{
	// (1) first two bytes of file are not used
	// (2) offset is in words (16 bit), so we double to find the byte
	// offset
	return (from << 1) + 2;
}

static FILE *fopen_safe(const char *fname, const char *mode)
{
	FILE *f = fopen(fname, mode);
	if (f == nullptr) {
		print_errno();
		fprintf(stderr, "fname = %s\n", fname);
		fatal("cannot open file");
	}
	return f;
}


int main(int argc, char **argv)
{
	if (argc <= 1) {
		fatal("needs a .binflow file as an argument");
	}
	FILE *f = fopen_safe(argv[1], "rb");
	fseek(f, 2, SEEK_SET);
	long offset = readf<uint32_t>(f);
	if (offset == 1) {
		fatal("cannot handle files this big yet, sorry !");
	}
	offset = calc_offset(offset);
	long header = ftell(f);
	fseek(f, offset, SEEK_SET);
	struct_defs = new Chunk(f);
	if (struct_defs->get_kind() != C_ARRAY) {
		fatal("struct defs is not an array");
	}
	{
		const std::vector<Chunk*> *const arr = struct_defs->as_array();
		for (size_t i = 0; i < arr->size(); ++i) {
			if (arr->at(i)->get_kind() != C_ARRAY) {
				fatal("one of items in the struct defs is not an array");
			} else if (arr->at(i)->as_array()->size() < 2) {
				fatal("one of items in the struct defs has an array length less than 2");
			}
			const int nargs = arr->at(i)->as_array()->at(0)->as_int();
			const char *name = arr->at(i)->as_array()->at(1)->as_string()->c_str();
			printf("line %d, nargs = %d, name = %s\n", __LINE__, nargs, name);
		}
	}
	fseek(f, header, SEEK_SET);
	root = new Chunk(f);
	fclose(f);
	FILE *json = fopen_safe("output.json", "w");
	root->to_json(json);
	fclose(json);
	FILE *html_ans = fopen_safe("output.ans.txt", "w");
	root->to_html_ans(html_ans);
	fclose(html_ans);

}
