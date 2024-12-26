#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

// ����
const size_t TS_PACKET_SIZE = 188; // TS ����С����λ���ֽڣ�
const uint8_t SYNC_BYTE = 0x47; // TS ����ͬ���ֽ�
const size_t WINDOW_SIZE = 5; // �������ڴ�С�����Ը�����Ҫ����

//��ȡ TS �ļ����������ֽ�����
std::vector<uint8_t> readTsFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        exit(1);
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    return data;
}

// д������� TS ���ݵ����ļ�
void writeAlignedTsFile(const std::string& filename, const std::vector<uint8_t>& alignedData) {
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to write to file: " << filename << std::endl;
        exit(1);
    }
    outFile.write(reinterpret_cast<const char*>(alignedData.data()), alignedData.size());
}
// ���������жϸ��ֽں��Ƿ񶪱���
bool isPacketLost(const std::vector<uint8_t>& originalData, const std::vector<uint8_t>& corruptedData, size_t originalPos, size_t corruptedPos) {
    size_t matchCount = 0;
    for (size_t i = 0; i < WINDOW_SIZE && originalPos + i < originalData.size() && corruptedPos + i < corruptedData.size(); ++i) {
        if (originalData[originalPos + i] == corruptedData[corruptedPos + i]) {
            matchCount++;
        }
    }
    return matchCount < WINDOW_SIZE / 2; // ���ƥ����ֽ������ڴ��ڴ�С��һ�룬��Ϊ�Ƕ�����
}
// ��TS������ȡPID
uint16_t Pid(const std::vector<uint8_t>& data, size_t pos) {

    // PIDռ13λ���ֱ�λ�ڵ�2�ֽڵĸ�5λ�͵�3�ֽ�
    uint16_t pid = ((data[pos + 1] & 0x1F) << 8) | data[pos + 2];
    return pid; // ����PID
}
bool Pid_sync(const std::vector<uint8_t>& data1, const std::vector<uint8_t>& data2, size_t pos1 ,size_t pos2) {
    // ��������������ȡPID
    uint16_t pid1 = Pid(data1, pos1);
    uint16_t pid2 = Pid(data2, pos2);

    // ��ȡ�����������ֽ�
    uint8_t byte1_1 = data1[pos1 + 3]; // header ���һ���ֽ�
    uint8_t byte1_2 = data1[pos1 + 4]; // payload ��һ���ֽ�
    uint8_t byte2_1 = data2[pos2 + 3];
    uint8_t byte2_2 = data2[pos2 + 4];

    // �Ƚ�PID�ͺ����ֽ�
    return (pid1 == pid2) && (byte1_1 == byte2_1) && (byte1_2 == byte2_2);

}
struct AlignedDataResult {
    std::vector<uint8_t> alignedData;
    std::vector<uint8_t> diffData;
};
    // �������� TS �ļ���ԭʼ TS �ļ�
AlignedDataResult alignTsFiles(const std::vector<uint8_t>& originalData, const std::vector<uint8_t>& corruptedData) {
    std::vector<uint8_t> alignedData(originalData.size(), 0); // Ԥ�����С����ʼ��Ϊ0
	std::vector<uint8_t> diffData(originalData.size(), 0); // Ԥ�����С����ʼ��Ϊ0
    size_t originalPos = 0;
    size_t corruptedPos = 0;
    // ����ԭʼ�ļ����ֽ�
    while (originalPos + TS_PACKET_SIZE <= originalData.size()) {

    // ����ͬ���ֽ� (0x47)��ȷ������Ч�� TS ��
    // ���Ⱥ�����������޷�������ʹ��ǰ��
	// ��ǰ��ɨ����ϣ��������pb��С�ˣ����pb����ֱ���˳�
        while (corruptedPos < corruptedData.size()) {
			if (corruptedData[corruptedPos] == SYNC_BYTE ){
                if ((corruptedPos + TS_PACKET_SIZE < corruptedData.size())){
                    if (corruptedData[corruptedPos + TS_PACKET_SIZE] == SYNC_BYTE || (corruptedPos - TS_PACKET_SIZE < corruptedData.size()
                        && corruptedData[corruptedPos - TS_PACKET_SIZE] == SYNC_BYTE)) {
						
						break;
                    }
                }
                else {
					if (corruptedData[corruptedPos - TS_PACKET_SIZE] == SYNC_BYTE) {
						break;
					}
                }
			
			}
            corruptedPos += 1;  
        }    


        //�ҵ���ͬ���ֽڣ�����PID��һ����˵�������˶�������Ҫ��originalData���ҵ���Ӧ�İ�
		if (corruptedPos < corruptedData.size() && (!Pid_sync(originalData, corruptedData, originalPos, corruptedPos))) {
            //std::cout<< "�ҵ���ͬ���ֽڣ�����PID��һ��" << std::endl;
			int pb = 0;
			while ( originalPos +  pb * TS_PACKET_SIZE <= originalData.size() ) {
                pb++;

				if (Pid_sync(originalData, corruptedData, originalPos + pb * TS_PACKET_SIZE, corruptedPos)){
					originalPos += pb  * TS_PACKET_SIZE;
                    break;
				}
				if (pb > 12) {  //����ܴ����������εĶ���
					corruptedPos += 1;
					break;
				}

             }
		}

        // �ҵ���ͬ���ֽڣ���PIDһ����˵������û�ж���
        if (corruptedPos < corruptedData.size() && Pid_sync(originalData, corruptedData, originalPos, corruptedPos)) {
            // ����¸���PID��һ����˵��û�ж���
            if (Pid_sync(originalData, corruptedData, originalPos + TS_PACKET_SIZE, corruptedPos + TS_PACKET_SIZE) &&
                originalPos + TS_PACKET_SIZE < originalData.size() &&
                corruptedPos + TS_PACKET_SIZE < corruptedData.size()
                ) {
                std::fill(alignedData.begin() + originalPos, alignedData.begin() + originalPos + TS_PACKET_SIZE, 0xff);
               // std::copy(corruptedData.begin() + corruptedPos, corruptedData.begin() + corruptedPos + TS_PACKET_SIZE, alignedData.begin() + originalPos);
                for (int i = 0; i < TS_PACKET_SIZE; i++) {
                    if (corruptedData[corruptedPos + i] != originalData[originalPos + i]) {
                        diffData[originalPos + i] = corruptedData[corruptedPos + i] ^ originalData[originalPos + i];
                        // diffData[originalPos + i] = !(corruptedData[corruptedPos + i] ^ originalData[originalPos + i]); // ͬ����ͬΪ1
                    }
                }
                corruptedPos += TS_PACKET_SIZE;

            }
            // �¸�����ͬ����˵�������˶����أ���Ҫ����رȽ�
            else {
                for (int i = 0; i < TS_PACKET_SIZE; i++) {
                    if (corruptedData[corruptedPos + i] != originalData[originalPos + i] && !isPacketLost(originalData, corruptedData, originalPos + i, corruptedPos + i)) {
                        diffData[originalPos + i] = corruptedData[corruptedPos + i] ^ originalData[originalPos + i];
                    }
                    else if (corruptedData[corruptedPos + i] != originalData[originalPos + i] && isPacketLost(originalData, corruptedData, originalPos + i, corruptedPos + i)) {
                        std::fill(alignedData.begin() + originalPos, alignedData.begin() + originalPos + i, 0xff);
                        break;
                    }
                    
                }
                corruptedPos += 1;
            }   

		}  
        originalPos += TS_PACKET_SIZE;
  

    }
    //��ӡoriginalPos��corruptedPos
    std::cout << "originalData size: " << originalPos << std::endl;
    std::cout << "corruptedData size: " << corruptedPos << std::endl;
    return { alignedData, diffData }; // ���ط�װ�Ľṹ��
}

int main() {
    // ��ȡԭʼ�ļ��������ļ�
    std::vector<uint8_t> originalData = readTsFile("orig.ts");
    std::vector<uint8_t> corruptedData = readTsFile("./Test_ERROR/orig_edit.ts");

    AlignedDataResult result = alignTsFiles(originalData, corruptedData);

    // д����������ݵ����ļ�
    writeAlignedTsFile("./Test_ERROR/test_error_aligned.ts", result.alignedData);
    writeAlignedTsFile("./Test_ERROR/test_error.ts", result.diffData);

    // algin��origin��λ��
    std::vector<uint8_t> alignedDataYu;
    size_t minSize = std::min(originalData.size(), result.alignedData.size());
    alignedDataYu.reserve(minSize); // Ԥ���ռ����������

    for (size_t i = 0; i < minSize; ++i) {
        alignedDataYu.push_back(originalData[i] & result.alignedData[i]);
    }

    // д�밴λ��Ľ�������ļ�
    writeAlignedTsFile("./Test_ERROR/test_error_aligned2.ts", alignedDataYu);

    std::cout << "TS files aligned and written to aligned.ts" << std::endl;

    return 0;
}
