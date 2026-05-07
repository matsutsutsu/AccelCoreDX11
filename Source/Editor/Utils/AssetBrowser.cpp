#include "AssetBrowser.h"
#include <imgui.h>
#include <DirectXTex.h>
#include <wrl/client.h>
#include <iostream>
#include <algorithm> // std::sort

#include <imgui_internal.h>

#include"ShellIconLoader.h"


using namespace DirectX;
using Microsoft::WRL::ComPtr;

//----------------------------------------------
// Splitter（左右・上下を分けるリサイズバー）
//----------------------------------------------
// split_vertically = true  → 垂直分割（左右ペインの調整）
// split_vertically = false → 水平分割（上下ペインの調整）
//
// thickness : バーの太さ（ピクセル）
// size0     : 左(または上)ペインのサイズ（参照渡し）
// size1     : 右(または下)ペインのサイズ（参照渡し）
// min_size0 : 左/上の最小サイズ
// min_size1 : 右/下の最小サイズ
//----------------------------------------------
static void Splitter(bool split_vertically, float thickness,
    float* size0, float* size1, float min_size0, float min_size1)
{
    // 現在のカーソル位置を退避（後で戻すため）
    ImVec2 backup_pos = ImGui::GetCursorPos();

    // 仕切り線（Buttonとして描画する）
    ImGui::Button("##splitter",
        split_vertically ? ImVec2(thickness, *size0 + *size1)   // 垂直分割 → 高さは両ペインの合計
        : ImVec2(*size0 + *size1, thickness)); // 水平分割 → 幅は両ペインの合計

    // マウスでドラッグされている間
    if (ImGui::IsItemActive())
    {
        if (split_vertically)
        {
            // X方向のマウス移動量を取得
            float delta = ImGui::GetIO().MouseDelta.x;

            // 最小サイズを下回らないように制限
            if (delta < min_size0 - *size0) delta = min_size0 - *size0;
            if (delta > *size1 - min_size1) delta = *size1 - min_size1;

            // 左右の幅を更新
            *size0 += delta;
            *size1 -= delta;
        }
        else
        {
            // Y方向のマウス移動量を取得
            float delta = ImGui::GetIO().MouseDelta.y;

            // 最小サイズを下回らないように制限
            if (delta < min_size0 - *size0) delta = min_size0 - *size0;
            if (delta > *size1 - min_size1) delta = *size1 - min_size1;

            // 上下の高さを更新
            *size0 += delta;
            *size1 -= delta;
        }
    }

    // カーソルを元の位置に戻す（仕切りの描画でズレないようにするため）
    ImGui::SetCursorPos(backup_pos);
}



//----------------------------------------------
// コンストラクタ
//----------------------------------------------
AssetBrowser::AssetBrowser(const std::string& rootPath, ID3D11Device* device)
	: rootPath(rootPath), device(device)
{
    // 拡張子 → カテゴリ のマップ
    extensionMap = {
        {".fbx", "model"},
        {".obj", "model"},
        {".glb", "model"},       
        {".png", "texture"},
        {".jpg", "texture"},
        {".jpeg", "texture"},
        {".cpp", "script"},
        {".h", "script"},
        {".json", "prefab"},
    };

    // 初期フィルタ設定（必要ならオンに）
    typeFilters["model"] = true;
    typeFilters["texture"] = true;
    typeFilters["script"] = true;
    typeFilters["prefab"] = true;

    LoadIcons(device);

    rootNode.name = rootPath;
    rootNode.path = rootPath;
    rootNode.isDirectory = true;

    LoadDirectory(rootPath, rootNode);
}


//----------------------------------------------
// ディレクトリ走査
//----------------------------------------------
void AssetBrowser::LoadDirectory(const fs::path& dirPath, AssetNode& node)
{
	// ディレクトリが存在しない場合は終了
	//     fs::exists はパスが存在するか確認
	//     fs::is_directory はパスがディレクトリか確認
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath))
        return;

	// ディレクトリ内の全エントリを走査
	// fs::directory_iterator はディレクトリ内のファイル・フォルダを列挙するイテレータ
    for (auto& entry : fs::directory_iterator(dirPath))
    {
        // 無効なエントリはスキップ 
        //      .batや.exeなど関係ないファイルをスキップするように
        if (!fs::exists(entry.path()))
            continue;

        // 子ノードを作成
        AssetNode child;
        child.name = entry.path().filename().u8string();
        child.path = entry.path();
        child.isDirectory = fs::is_directory(entry.path());

		// フォルダなら再帰的に読み込み
        if (child.isDirectory)
            LoadDirectory(entry.path(), child);     //再帰的に処理       
        else
        {
            // 有効なファイルだけ追加（中身がないファイルはスキップ）
			//    変なファイルをスキップするためにファイルサイズをチェック
            std::error_code ec;
            if (fs::file_size(entry.path(), ec) && !ec) // ファイルサイズチェック
            {
                node.children.push_back(std::move(child));
            }
            continue;
        }

		// 作った子ノードを親ノードに追加
        node.children.push_back(std::move(child));
    }

    // フォルダ優先 + 名前順にソート
	//      フォルダを先に、その後に名前順でソート
    std::sort(node.children.begin(), node.children.end(),
        [](const AssetNode& a, const AssetNode& b) {
            if (a.isDirectory != b.isDirectory)
                return a.isDirectory > b.isDirectory; // フォルダ優先
            return a.name < b.name; // 名前順
        });
}


//----------------------------------------------
// アイコンロード
//  ・DirectXTexを使って画像を読み込み、DirectXのテクスチャとしてSRVを作成
//	・iconMapに保存する
//----------------------------------------------
AssetBrowser::IconTexture AssetBrowser::LoadIcon(ID3D11Device* device, const std::wstring& path)
{
    IconTexture icon;
	TexMetadata metadata;   // 画像のメタデータ（幅・高さ・フォーマットなど）
	ScratchImage image;     // 画像データを一時的に保持するオブジェクト

	// WIC経由で画像を読み込み (Windows Imaging Component)
	// 　　pngやjpgなどをでコードしてimageに格納、失敗したら終了
    HRESULT hr = LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, &metadata, image);
    if (FAILED(hr))
    {
        std::wcout << L"Failed to load icon: " << path << std::endl;
        return icon;
    }

	// ScratchImageからD3D11のSRVを作成
    ComPtr<ID3D11ShaderResourceView> tmpSrv;
    hr = CreateShaderResourceView(device, image.GetImages(), image.GetImageCount(), metadata, tmpSrv.GetAddressOf());
    if (FAILED(hr)) {
        std::wcerr << L"Failed to create SRV for icon: " << path << std::endl;
        return icon;
    }

    icon.srv = tmpSrv; // ComPtr をコピー -> 参照カウントを保持
    icon.width = static_cast<int>(metadata.width);
    icon.height = static_cast<int>(metadata.height);
    return icon;
}

void AssetBrowser::LoadIcons(ID3D11Device* device)
{
    //このファイルパス後で変更する
    iconMap["folder"] = LoadIcon(device, L"./Icons/TestImage.png");
    iconMap["model"] = LoadIcon(device, L"./Icons/TestImage.png");
    iconMap["texture"] = LoadIcon(device, L"./Icons/TestImage.png");
    iconMap["script"] = LoadIcon(device, L"./Icons/TestImage.png");
    iconMap["prefab"] = LoadIcon(device, L"./Icons/TestImage.png"); 
}


//----------------------------------------------
// パスからノードを探す
//----------------------------------------------
AssetBrowser::AssetNode* AssetBrowser::FindNodeByPath(AssetNode& node, const fs::path& path)
{
    if (node.path == path) return &node;
    for (auto& child : node.children)
    {
        if (auto found = FindNodeByPath(child, path))
            return found;
    }
    return nullptr;
}


//----------------------------------------------
// 右ペイン：グリッド
//----------------------------------------------
void AssetBrowser::DrawGridView(AssetNode& node)
{
    // ---- 表示するフォルダを決定 ----
    AssetNode* folder = selectedFolder ? selectedFolder : &node;

    //if (!selectedFilePath.empty()) {
    //    // 現在選択しているファイルの「親フォルダ」を探す
    //    fs::path parent = fs::path(selectedFilePath).parent_path();
    //    if (auto found = FindNodeByPath(rootNode, parent))
    //        folder = found; // 見つかればそのフォルダを表示対象にする
    //}

    // ---- アイコンサイズをユーザーが調整できるスライダー ----
    static float iconSize = 64.0f; // デフォルト64px
    ImGui::SliderFloat("Icon Size", &iconSize, 32.0f, 128.0f, "%.0f");

    // --- アイコンサイズに応じて余白を設定 ---
    float padding = iconSize * 0.15f;   // セル内余白
    float spacing = iconSize * 0.1f;    // アイコン間の余白

    // アイコンサイズ＋余白で列数計算
    int columns = (std::max)(1, (int)(ImGui::GetContentRegionAvail().x / (iconSize + spacing * 2)));


    // ---- テーブル（グリッド）の開始 ----
    // 5列のテーブルで、セル幅は均等に調整
    if (ImGui::BeginTable("Grid", columns, ImGuiTableFlags_SizingStretchSame))
    {
        // フォルダ内の子要素を順に描画
        for (auto& child : folder->children)
        {
            // フィルタリングされたファイルだけ表示
            if (!FilterAsset(child)) continue;
            // 次の列へ
            ImGui::TableNextColumn();
            // 子要素ごとにユニークなIDを持たせる（ImGuiのID衝突回避）
            ImGui::PushID(child.path.string().c_str());

            //  ShellIconLoaderを使ってシェルアイコン取得
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iconSrv;

            //  フォルダなら専用取得
            if (child.isDirectory)
            {
                iconSrv = ShellIconLoader::GetIconSRV(child.path.wstring(), device);
            }


            bool isSelected = (selectedFilePath == child.path.string());
            if (isSelected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 0.5f));


            // ---- アイコン表示（ImageButton or 代替）----
            if (iconSrv)
            {
                // アイコンをImageButtonとして描画
                if (ImGui::ImageButton("##icon", (ImTextureID)iconSrv.Get(),
                    ImVec2(iconSize, iconSize)))
                {
                    if (child.isDirectory)
                        selectedFolder = &child; // ←フォルダなら移動
                    else
                        selectedFilePath = child.path.string();
                }
            }
            else
            {
                // アイコンが見つからない場合は「?」ボタンを表示
                ImGui::Button("?", ImVec2(iconSize, iconSize));
            }

            // 選択状態用に Push した色を戻す
            if (isSelected) ImGui::PopStyleColor();

            // ドラッグ＆ドロップの「送り手（Source）」になる
            if (!child.isDirectory) {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    std::string filePath = child.path.string();
                    if (filePath.find(".json") != std::string::npos) {
                        ImGui::SetDragDropPayload("ASSET_PREFAB", filePath.c_str(),
                                                  filePath.size() + 1);
                        ImGui::Text("Spawn Prefab: %s", child.name.c_str());
                    }
                    else if (filePath.find(".glb") != std::string::npos ||
                                 filePath.find(".gltf") != std::string::npos) {
                        ImGui::SetDragDropPayload("ASSET_MODEL", filePath.c_str(),
                                                  filePath.size() + 1);
                        ImGui::Text("Load Model: %s", child.name.c_str());
                    }
                    ImGui::EndDragDropSource();
                }
            }

            // ファイル名を表示（長い場合は折り返し）
            ImGui::TextWrapped("%s", child.name.c_str());

            // IDを戻す（PushIDとペア）
            ImGui::PopID();
        }
        ImGui::EndTable(); // テーブルを閉じる
    }
}

// 再帰的にノードを描画
//フォルダなら ImGui::TreeNode で「 開閉可能ツリー」表示。
//ファイルなら ImGui::Selectable で「選択可能テキスト」として表示。
//さらに ImGui::ImageButton を使って アイコンの大きいプレビューもクリックできる。
void AssetBrowser::DrawAssetNode(AssetNode& node)
{
	// 内部でウィジットを識別するために一意のIDをプッシュ
    // 同じ表示名が複数ある時に衝突を避けるため
    std::string id = node.path.string();    //オブジェクトが消えないように一時保存
    ImGui::PushID(id.c_str()); // ①一意のID

    if (!FilterAsset(node)) { ImGui::PopID(); return; } // ②フィルタで非表示なら帰る
    // ※PushID したら必ず PopID() を呼ぶ

    // アイコン選択（ComPtrを前提）
	//      ノードの種類（フォルダ / ファイル拡張子）に応じて表示するアイコンを決める
// フォルダなら ShellIconLoader を使ってアイコン取得
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iconSrv = nullptr;
    if (node.isDirectory)
    {
        iconSrv = ShellIconLoader::GetIconSRV(node.path.wstring(), device);
    }

	// フォルダならツリー表示、ファイルなら選択可能テキスト表示
    if (node.isDirectory)
    {
        /*flags の意味：
            ImGuiTreeNodeFlags_OpenOnArrow：三角（矢印）をクリックして開閉できる（ラベルをクリックして開閉する方式と切り分けられる）。
            ImGuiTreeNodeFlags_SpanAvailWidth：ノードのラベル領域を利用可能幅いっぱいに使う（見た目の幅を引き伸ばす）。
            ↑の引き延ばすやつ使うとクリックできなくなるので消した*/
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;

        // ユニークなラベル（表示名とIDを分離）
        std::string treeNodeLabel = "##" + id;

        // ラベルは空文字にして、自分で中身を描画する
        bool open = ImGui::TreeNodeEx(treeNodeLabel.c_str(), flags, "");

        // ←ここでラベル部分に好きなUIを描画できる
        if (iconSrv) {
            ImGui::SameLine();
            ImGui::Image((ImTextureID)iconSrv.Get(), ImVec2(16, 16));
        }
        ImGui::SameLine();
 
        // ---- フォルダクリックで選択フォルダを更新 ----
        std::string selectableLabel = node.name + "##" + id;
        bool isSelected = (selectedFolder == &node);
        if (ImGui::Selectable(selectableLabel.c_str(), isSelected))
        {
            selectedFolder = &node;
        }

		// open が true なら子ノードを描画
		// ImGui::TreePop() でツリーの階層を戻す
        if (open) {
            for (auto& child : node.children)
                DrawAssetNode(child);
            ImGui::TreePop();
        }
    }
    else
    {
        float IndentValue = 35.0f;

        ImGui::Indent(IndentValue); // 階層インデント（フォルダの中に属している感を出す）

        if (iconSrv)
            ImGui::Image((ImTextureID)iconSrv.Get(), ImVec2(16, 16));

        ImGui::SameLine();

        if (ImGui::Selectable(node.name.c_str(), selectedFilePath == node.path.string())) {
            selectedFilePath = node.path.string();
        }

        // ★ 追加：ツリー側のドラッグソース
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            std::string filePath = node.path.string();
            if (filePath.find(".json") != std::string::npos) {
                ImGui::SetDragDropPayload("ASSET_PREFAB", filePath.c_str(), filePath.size() + 1);
                ImGui::Text("Spawn Prefab: %s", node.name.c_str());
            }
            else if (filePath.find(".glb") != std::string::npos || filePath.find(".gltf") != std::string::npos) {
                ImGui::SetDragDropPayload("ASSET_MODEL", filePath.c_str(), filePath.size() + 1);
                ImGui::Text("Load Model: %s", node.name.c_str());
            }
            ImGui::EndDragDropSource();
        }

        ImGui::Unindent(IndentValue); // インデントを戻す

    }


    ImGui::PopID();
}



//----------------------------------------------
// メイン描画
//----------------------------------------------
void AssetBrowser::Draw()
{
    ImGui::Begin("Asset Browser");

    DrawSearchBar();

	static float leftWidth = 200.0f;  //左ペインの初期幅
	static float rightWidth = 400.0f; //右ペインの初期幅
	float totalWidth = ImGui::GetContentRegionAvail().x; //利用可能な幅

	if (leftWidth + rightWidth < totalWidth)    // ウィンドウが広がったら
        rightWidth = totalWidth - leftWidth;

	// 左ペイン　領域確保
    ImGui::BeginChild("LeftPane", ImVec2(leftWidth, 0), true);
    DrawAssetNode(rootNode);   // ← フォルダツリーを描画
    ImGui::EndChild();

	// 中央の分割バー
    ImGui::SameLine();
	// 自作関数でマウスドラッグするとleftWidth/rightWidthを調整できる
	// 第２引数は「仕切り線の太さ」第３引数以降は「最小幅」
    Splitter(true, 4.0f, &leftWidth, &rightWidth, 100.0f, 100.0f);
    ImGui::SameLine();

    ImGui::BeginChild("RightPane", ImVec2(rightWidth, 0), true);
    DrawGridView(rootNode);   // ← グリッド表示用の関数
    ImGui::EndChild();

    

    ImGui::End();
}



//----------------------------------------------
// 検索バー
//----------------------------------------------
void AssetBrowser::DrawSearchBar()
{
	// InputTextはChar型の配列を要求するので変換
    char buffer[256];
    strncpy_s(buffer, sizeof(buffer), searchQuery.c_str(), _TRUNCATE);
    if (ImGui::InputText("Search", buffer, sizeof(buffer)))
    {
        searchQuery = buffer;
    }

    ImGui::Text("Filter by type:");
    ImGui::Checkbox(".fbx/.obj/.glb", &typeFilters["model"]);
    ImGui::SameLine();
    ImGui::Checkbox(".png/.jpg", &typeFilters["texture"]);
    ImGui::SameLine();
    ImGui::Checkbox(".cpp/.h", &typeFilters["script"]);
    ImGui::SameLine();
    ImGui::Checkbox(".json", &typeFilters["prefab"]);
}

//----------------------------------------------
// フィルタ判定
//----------------------------------------------
bool AssetBrowser::FilterAsset(const AssetNode& node)
{
    // フォルダは常に表示
    if (node.isDirectory)
        return true;

    // 2. 検索バーのフィルタリング
    if (!searchQuery.empty()) {
        std::string nameLower = node.name;
        std::string queryLower = searchQuery;
        // 大文字小文字を区別せずに検索できるように小文字化
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);
        
        if (nameLower.find(queryLower) == std::string::npos) {
            return false; // 検索文字が含まれていなければ非表示
        }
    }

    // 3. 拡張子（チェックボックス）のフィルタリング
    auto ext = node.path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // 拡張子を小文字化 (例: .JSON -> .json)

    auto it = extensionMap.find(ext);
    if (it != extensionMap.end()) {
        const std::string& type = it->second;
        // そのカテゴリのチェックボックスが外れていたら非表示
        if (typeFilters.count(type) && !typeFilters[type]) {
            return false;
        }
    } else {
        // ★重要: extensionMap に登録されていない謎の拡張子（.metaなど）はノイズになるので隠す
        return false;
    }

    // すべての条件をクリアしたファイルだけを表示
    return true;
}
