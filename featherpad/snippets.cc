#include <stdlib.h>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>


std::vector<std::tuple<QString,QString,int,int>> snippet_list;


/*
 ******************************************************************************************
 * Place your custom text snippets in ~/.config/featherpad/snippets.json file with following structure:
 * {
 *	<key_binding>: {
 *		"str": <your_snippet>,
 *		"hor": <horizontal_cursor_offset (negative - go left, positive - right)>,
 *		"vert": <vertical_cursor_offset (negative - go up, positive - down)>
 *	}
 * }
 *******************************************************************************************
 */
void
parse_snippets_file()
{
	char home_buf[1024];
	strcpy( home_buf, getenv("HOME") );
	QString snippets_filepath = (
		strcat( home_buf , "/.config/featherpad/snippets.json" )
	);
	QFile snippets_file;
	snippets_file.setFileName(  snippets_filepath  );
	
	snippets_file.open(QIODevice::ReadOnly);
	
	QByteArray snippets_json_raw = snippets_file.readAll();
	snippets_file.close();
	
	QJsonDocument doc( QJsonDocument::fromJson( snippets_json_raw ) );
	QJsonObject snippets_json = doc.object();
	
	foreach( const QString& key, snippets_json.keys(  ) ){
		QJsonValue value = snippets_json.value( key );
		QJsonObject shortcut_data = value.toObject();
		
		QString str = shortcut_data.value( "str" ).toString();
		int off_hor = ( shortcut_data.value( "hor" ) ).toInt();
		int off_vert = ( shortcut_data.value("vert") ).toInt();
		
		snippet_list.insert(snippet_list.end(), {key,str,off_vert,off_hor});
		
	};
	
};
